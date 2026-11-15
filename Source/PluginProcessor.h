#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "CurveModel.h"

class NoktaPulseProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBins = 512;
    static constexpr std::array<float, 8> divisionQuarters { 4.f, 2.f, 1.f, .5f, .25f, 2.f / 3.f, 1.f / 3.f, 1.f / 6.f };
    static juce::StringArray divisionNames() { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/16T" }; }

    NoktaPulseProcessor();
    ~NoktaPulseProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nokta Pulse"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Curva (hilo de interfaz)
    std::vector<CurvePoint> getCurve() const;
    void setCurve (std::vector<CurvePoint> points);
    int getCurveVersion() const { return curveVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;

    // Datos para la interfaz (escritos desde el hilo de audio)
    std::atomic<float> grDb { 0.f }, outPeakL { 0.f }, outPeakR { 0.f }, displayPhase { 0.f };
    std::array<std::atomic<float>, numBins> waveBins;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *inParam, *outParam, *timeParam, *offsetParam, *modeParam, *threshParam;

    mutable juce::SpinLock curveLock;
    std::vector<CurvePoint> curve;
    CurveModel::Table table {};
    std::atomic<int> curveVersion { 0 };
    CurveModel::Table audioTable {};
    int audioVersion = -1;

    double sampleRate = 44100.0;
    double phase = 0.0, oneShotPos = 2.0;
    float smoothedGain = 1.f, smoothCoef = 0.01f, env = 0.f, envDecay = 0.999f;
    bool armed = true;
    int holdoff = 0, holdoffSamples = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoktaPulseProcessor)
};
