#include "PluginEditor.h"

namespace
{
    constexpr float designW = 1672.f, designH = 941.f;

    float peakToFrac (float lin)
    {
        const float db = juce::Decibels::gainToDecibels (lin, -60.f);
        return juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float ch = 28.f)
    {
        using namespace juce;
        Path p;
        p.startNewSubPath (r.getX() + ch, r.getY());
        p.lineTo (r.getRight() - ch, r.getY());
        p.lineTo (r.getRight(), r.getY() + ch);
        p.lineTo (r.getRight(), r.getBottom() - ch);
        p.lineTo (r.getRight() - ch, r.getBottom());
        p.lineTo (r.getX() + ch, r.getBottom());
        p.lineTo (r.getX(), r.getBottom() - ch);
        p.lineTo (r.getX(), r.getY() + ch);
        p.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xff0d0a1a), r.getX(), r.getY(), Colour (0xff07050f), r.getRight(), r.getBottom(), false));
        g.fillPath (p);
        g.setGradientFill (ColourGradient (Colour (0xff9a35ff), r.getX(), r.getY(), Colour (0xff4a1a96), r.getRight(), r.getBottom(), false));
        g.strokePath (p, PathStrokeType (1.5f));
    }

    void drawText (juce::Graphics& g, const juce::String& t, float x, float y, float w, float h,
                   float size, float kerning, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (c);
        g.setFont (juce::Font (juce::FontOptions (size)).withExtraKerningFactor (kerning));
        g.drawText (t, juce::Rectangle<float> (x, y, w, h), j);
    }
}

NoktaPulseEditor::NoktaPulseEditor (NoktaPulseProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), curveEditor (p), strip (p),
      grMeter ([&p] { return juce::jlimit (0.f, 1.f, -p.grDb.load() / 30.f); }),
      outLMeter ([&p] { return peakToFrac (p.outPeakL.load()); }),
      outRMeter ([&p] { return peakToFrac (p.outPeakR.load()); })
{
    setLookAndFeel (&laf);

    addAndMakeVisible (curveEditor);
    addAndMakeVisible (strip);
    addAndMakeVisible (grMeter);
    addAndMakeVisible (outLMeter);
    addAndMakeVisible (outRMeter);

    setupKnob (inSlider, NoktaColours::cyan, 0.0);
    setupKnob (offsetSlider, NoktaColours::purple, 0.0);
    setupKnob (outSlider, NoktaColours::cyan, 0.0);
    setupKnob (threshSlider, NoktaColours::purple, -24.0);

    timeBox.addItemList (NoktaPulseProcessor::divisionNames(), 1);
    modeBox.addItemList ({ "Sync", "MIDI", "Audio" }, 1);
    addAndMakeVisible (timeBox);
    addAndMakeVisible (modeBox);

    grLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (grLabel);

    inAtt     = std::make_unique<SliderAtt> (proc.apvts, "in", inSlider);
    offsetAtt = std::make_unique<SliderAtt> (proc.apvts, "offset", offsetSlider);
    outAtt    = std::make_unique<SliderAtt> (proc.apvts, "out", outSlider);
    threshAtt = std::make_unique<SliderAtt> (proc.apvts, "thresh", threshSlider);
    timeAtt   = std::make_unique<ComboAtt> (proc.apvts, "time", timeBox);
    modeAtt   = std::make_unique<ComboAtt> (proc.apvts, "mode", modeBox);

    modeBox.onChange = [this] { threshSlider.setEnabled (modeBox.getSelectedItemIndex() == 2); };
    modeBox.onChange();

    for (int i = 0; i < CurveModel::numPresets; ++i)
    {
        auto& t = tiles[(size_t) i];
        t = std::make_unique<PresetTile> (i);
        t->onClick = [this] (int idx)
        {
            proc.setCurve (CurveModel::preset (idx));
            curveEditor.reload();
            updateTiles();
        };
        addAndMakeVisible (*t);
    }
    updateTiles();

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (836, 471, 2508, 1412);
    setSize (1254, 706);
    startTimerHz (30);
}

NoktaPulseEditor::~NoktaPulseEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NoktaPulseEditor::setupKnob (juce::Slider& s, juce::Colour accent, double dbl)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 24);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    s.setDoubleClickReturnValue (true, dbl);
    addAndMakeVisible (s);
}

void NoktaPulseEditor::updateTiles()
{
    lastCurveVersion = proc.getCurveVersion();
    const auto c = proc.getCurve();
    for (int i = 0; i < CurveModel::numPresets; ++i)
        tiles[(size_t) i]->setActive (CurveModel::equal (c, CurveModel::preset (i)));
}

void NoktaPulseEditor::timerCallback()
{
    curveEditor.tick();
    strip.repaint();
    grMeter.repaint();
    outLMeter.repaint();
    outRMeter.repaint();
    grLabel.setText (juce::String (proc.grDb.load(), 1) + " dB", juce::dontSendNotification);
    if (proc.getCurveVersion() != lastCurveVersion) updateTiles();
}

void NoktaPulseEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff1a0c33), b.getCentreX(), 0.f, Colour (0xff07050f),
                                       b.getCentreX(), b.getHeight() * 0.7f, true));
    g.fillAll();

    const float s = (float) getWidth() / designW;
    g.addTransform (AffineTransform::scale (s));

    const auto light = Colour (0xffece8fb);
    drawText (g, "NOKTA-PULSE", 70.f, 38.f, 700.f, 60.f, 38.f, 0.25f, light);
    drawText (g, "Nokta", 1414.f, 40.f, 150.f, 40.f, 28.f, 0.08f, light);

    g.setColour (NoktaColours::purple);
    g.drawEllipse (1591.f, 49.f, 30.f, 30.f, 6.f);
    g.setColour (NoktaColours::cyan);
    g.fillEllipse (1601.f, 59.f, 10.f, 10.f);
    Path claws;
    claws.startNewSubPath (1584.f, 30.f); claws.quadraticTo (1596.f, 40.f, 1598.f, 50.f);
    claws.startNewSubPath (1628.f, 28.f); claws.quadraticTo (1614.f, 38.f, 1612.f, 50.f);
    g.setColour (NoktaColours::purple);
    g.strokePath (claws, PathStrokeType (5.f));

    g.setGradientFill (ColourGradient (Colours::transparentBlack, 0.f, 0.f, NoktaColours::purple, 620.f, 0.f, false));
    g.fillRect (0.f, 112.f, 620.f, 2.f);

    drawPanel (g, { 24.f, 150.f, 290.f, 765.f });
    drawPanel (g, { 340.f, 150.f, 1010.f, 640.f });
    drawPanel (g, { 1376.f, 150.f, 272.f, 640.f });

    const auto txt = NoktaColours::text;
    drawText (g, "IN", 68.f, 184.f, 200.f, 30.f, 20.f, 0.28f, light);
    drawText (g, "TIME", 68.f, 462.f, 200.f, 30.f, 20.f, 0.28f, light);
    drawText (g, "OFFSET", 24.f, 578.f, 290.f, 24.f, 16.f, 0.18f, txt, Justification::centred);
    drawText (g, "REDUCTION", 124.f, 752.f, 180.f, 24.f, 16.f, 0.18f, txt);
    drawText (g, "OUT", 1420.f, 184.f, 200.f, 30.f, 20.f, 0.28f, light);
    drawText (g, "TRIGGER", 760.f, 32.f, 190.f, 22.f, 15.f, 0.18f, txt);
    drawText (g, "THRESH", 1096.f, 60.f, 100.f, 24.f, 15.f, 0.18f, txt);
}

void NoktaPulseEditor::resized()
{
    const float s = (float) getWidth() / designW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    for (auto* sl : { &inSlider, &offsetSlider, &outSlider, &threshSlider })
        sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::roundToInt (90 * s), juce::roundToInt (24 * s));

    inSlider.setBounds (R (94, 224, 150, 172));
    timeBox.setBounds (R (64, 514, 210, 56));
    offsetSlider.setBounds (R (119, 606, 100, 108));
    grMeter.setBounds (R (68, 740, 40, 150));
    grLabel.setBounds (R (124, 780, 160, 30));

    curveEditor.setBounds (R (360, 170, 970, 520));
    strip.setBounds (R (360, 706, 970, 56));

    outSlider.setBounds (R (1437, 400, 150, 176));
    outLMeter.setBounds (R (1468, 620, 34, 130));
    outRMeter.setBounds (R (1522, 620, 34, 130));

    modeBox.setBounds (R (760, 58, 190, 48));
    threshSlider.setBounds (R (1000, 22, 90, 94));

    for (size_t i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (R (340.f + 190.f * (float) i, 815, 170, 100));
}
