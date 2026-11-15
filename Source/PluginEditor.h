#pragma once
#include "PluginProcessor.h"
#include "Components.h"

class NoktaPulseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NoktaPulseEditor (NoktaPulseProcessor&);
    ~NoktaPulseEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void updateTiles();
    void setupKnob (juce::Slider&, juce::Colour, double doubleClickValue);

    NoktaPulseProcessor& proc;
    NoktaLookAndFeel laf;

    CurveEditor curveEditor;
    TimelineStrip strip;
    MeterBar grMeter, outLMeter, outRMeter;
    juce::Slider inSlider, offsetSlider, outSlider, threshSlider;
    juce::ComboBox timeBox, modeBox;
    juce::Label grLabel;
    std::array<std::unique_ptr<PresetTile>, (size_t) CurveModel::numPresets> tiles;
    int lastCurveVersion = -1;

    // Las asociaciones se declaran al final para destruirse antes que los controles
    std::unique_ptr<SliderAtt> inAtt, offsetAtt, outAtt, threshAtt;
    std::unique_ptr<ComboAtt> timeAtt, modeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoktaPulseEditor)
};
