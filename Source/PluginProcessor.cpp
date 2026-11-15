#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout NoktaPulseProcessor::createLayout()
{
    using namespace juce;
    auto oneDec = [] (float v, int) { return String (v, 1); };
    auto percent = [] (float v, int) { return String (roundToInt (v * 100.f)) + " %"; };

    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "in", 1 }, "Input",
        NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (oneDec)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output",
        NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (oneDec)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "time", 1 }, "Time", divisionNames(), 2));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "offset", 1 }, "Offset",
        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f,
        AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Trigger",
        StringArray { "Sync", "MIDI", "Audio" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "thresh", 1 }, "Threshold",
        NormalisableRange<float> (-60.f, 0.f, 0.1f), -24.f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (oneDec)));
    return { p.begin(), p.end() };
}

NoktaPulseProcessor::NoktaPulseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    inParam     = apvts.getRawParameterValue ("in");
    outParam    = apvts.getRawParameterValue ("out");
    timeParam   = apvts.getRawParameterValue ("time");
    offsetParam = apvts.getRawParameterValue ("offset");
    modeParam   = apvts.getRawParameterValue ("mode");
    threshParam = apvts.getRawParameterValue ("thresh");

    curve = CurveModel::preset (0);
    CurveModel::buildTable (curve, table);
    audioTable = table;
    for (auto& b : waveBins) b.store (0.f);
}

bool NoktaPulseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != layouts.getMainInputChannelSet()) return false;
    if (mainOut != juce::AudioChannelSet::stereo() && mainOut != juce::AudioChannelSet::mono()) return false;

    const auto sc = layouts.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

void NoktaPulseProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    smoothCoef = 1.f - std::exp (-1.f / (0.0008f * (float) sr));
    envDecay = std::exp (-1.f / (0.010f * (float) sr));
    holdoffSamples = (int) (0.03 * sr);
    smoothedGain = 1.f;
    env = 0.f;
    armed = true;
    holdoff = 0;
    oneShotPos = 2.0;
}

std::vector<CurvePoint> NoktaPulseProcessor::getCurve() const
{
    juce::SpinLock::ScopedLockType l (curveLock);
    return curve;
}

void NoktaPulseProcessor::setCurve (std::vector<CurvePoint> pts)
{
    if (pts.size() < 2) pts = CurveModel::preset (0);
    for (auto& p : pts)
    {
        p.x = juce::jlimit (0.f, 1.f, p.x);
        p.y = juce::jlimit (0.f, 1.f, p.y);
        p.c = juce::jlimit (-1.5f, 1.5f, p.c);
    }
    CurveModel::sortPoints (pts);
    pts.front().x = 0.f;
    pts.back().x = 1.f;

    CurveModel::Table t;
    CurveModel::buildTable (pts, t);
    {
        juce::SpinLock::ScopedLockType l (curveLock);
        curve = std::move (pts);
        table = t;
    }
    ++curveVersion;
}

void NoktaPulseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numMain = juce::jmin (getMainBusNumInputChannels(), getMainBusNumOutputChannels(), 2);
    if (numMain == 0 || numSamples == 0) return;

    // Tabla de la curva (copia sin bloquear el hilo de audio)
    const int v = curveVersion.load();
    if (v != audioVersion)
    {
        juce::SpinLock::ScopedTryLockType l (curveLock);
        if (l.isLocked()) { audioTable = table; audioVersion = v; }
    }

    // Sidechain
    const float* scPtr[2] { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        scCh = juce::jmin (2, scBuf.getNumChannels());
        for (int c = 0; c < scCh; ++c) scPtr[c] = scBuf.getReadPointer (c);
    }
    const bool hasSC = scCh > 0;

    // Informacion del host
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) { ppq = *q; havePpq = true; }
            playing = pos->getIsPlaying();
        }
    if (bpm < 1.0) bpm = 120.0;

    const float inGain  = juce::Decibels::decibelsToGain (inParam->load());
    const float outGain = juce::Decibels::decibelsToGain (outParam->load());
    const float thr     = juce::Decibels::decibelsToGain (threshParam->load());
    const int timeIdx   = juce::jlimit (0, 7, (int) timeParam->load());
    const int mode      = (int) modeParam->load();
    const double offset = (double) offsetParam->load();
    const double cycleQ = (double) divisionQuarters[(size_t) timeIdx];
    const double inc    = (bpm / 60.0 / sampleRate) / cycleQ;

    if (mode == 0 && playing && havePpq)
    {
        const double b = ppq / cycleQ;
        phase = b - std::floor (b);
    }

    float* ch[2] { buffer.getWritePointer (0), numMain > 1 ? buffer.getWritePointer (1) : nullptr };
    auto midiIt = midi.begin();
    float minGain = 1.f, peakL = 0.f, peakR = 0.f, lastDisplay = 0.f;

    for (int i = 0; i < numSamples; ++i)
    {
        bool trig = false;
        if (mode == 1)
            while (midiIt != midi.end() && (*midiIt).samplePosition <= i)
            {
                if ((*midiIt).getMessage().isNoteOn()) trig = true;
                ++midiIt;
            }

        // Nivel de la fuente de disparo / visualizacion
        float lvl = 0.f;
        if (hasSC) { for (int c = 0; c < scCh; ++c) lvl = juce::jmax (lvl, std::abs (scPtr[c][i])); }
        else       { for (int c = 0; c < numMain; ++c) lvl = juce::jmax (lvl, std::abs (ch[c][i])); }

        if (mode == 2)
        {
            env = juce::jmax (lvl, env * envDecay);
            if (holdoff > 0) --holdoff;
            if (env > thr && armed && holdoff <= 0) { trig = true; armed = false; holdoff = holdoffSamples; }
            if (env < thr * 0.5f) armed = true;
        }
        if (trig && mode != 0) oneShotPos = -offset;

        double eff = 0.0;
        bool shape = true;
        if (mode == 0)
        {
            double ph = phase - offset;
            ph -= std::floor (ph);
            eff = ph;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
            lastDisplay = (float) eff;
        }
        else
        {
            if (oneShotPos < 0.0) { shape = false; lastDisplay = 0.f; }
            else { eff = juce::jmin (oneShotPos, 1.0); lastDisplay = (float) eff; }
            oneShotPos = juce::jmin (oneShotPos + inc, 2.0);
        }

        float target = 1.f;
        if (shape)
        {
            const float pos = (float) eff * (float) CurveModel::tableSize;
            const int i0 = juce::jmin ((int) pos, CurveModel::tableSize - 1);
            const float f = pos - (float) i0;
            target = audioTable[(size_t) i0] + (audioTable[(size_t) i0 + 1] - audioTable[(size_t) i0]) * f;

            const int bin = juce::jlimit (0, numBins - 1, (int) (eff * numBins));
            const float old = waveBins[(size_t) bin].load (std::memory_order_relaxed);
            waveBins[(size_t) bin].store (juce::jmax (lvl * inGain, old * 0.9995f), std::memory_order_relaxed);
        }

        smoothedGain += (target - smoothedGain) * smoothCoef;
        minGain = juce::jmin (minGain, smoothedGain);

        const float g = inGain * smoothedGain * outGain;
        ch[0][i] *= g;
        peakL = juce::jmax (peakL, std::abs (ch[0][i]));
        if (ch[1] != nullptr)
        {
            ch[1][i] *= g;
            peakR = juce::jmax (peakR, std::abs (ch[1][i]));
        }
        else peakR = peakL;
    }

    grDb.store (juce::Decibels::gainToDecibels (minGain, -60.f));
    displayPhase.store (lastDisplay);
    outPeakL.store (juce::jmax (peakL, outPeakL.load() * 0.85f));
    outPeakR.store (juce::jmax (peakR, outPeakR.load() * 0.85f));
}

juce::AudioProcessorEditor* NoktaPulseProcessor::createEditor() { return new NoktaPulseEditor (*this); }

void NoktaPulseProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    auto* cx = xml->createNewChildElement ("CURVE");
    for (auto& p : getCurve())
    {
        auto* e = cx->createNewChildElement ("P");
        e->setAttribute ("x", (double) p.x);
        e->setAttribute ("y", (double) p.y);
        e->setAttribute ("c", (double) p.c);
    }
    copyXmlToBinary (*xml, dest);
}

void NoktaPulseProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    std::vector<CurvePoint> pts;
    if (auto* cx = xml->getChildByName ("CURVE"))
    {
        for (auto* e : cx->getChildIterator())
            pts.push_back ({ (float) e->getDoubleAttribute ("x"), (float) e->getDoubleAttribute ("y"),
                             (float) e->getDoubleAttribute ("c") });
        xml->removeChildElement (cx, true);
    }
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    if (pts.size() >= 2) setCurve (pts);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NoktaPulseProcessor(); }
