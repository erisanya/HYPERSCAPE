#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class HyperScapeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit HyperScapeAudioProcessorEditor(HyperScapeAudioProcessor&);
    ~HyperScapeAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HyperScapeAudioProcessor& audioProcessor;

    juce::Slider mixKnob;
    juce::Label title;
    juce::Label subtitle;
    juce::Label mixCaption;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    // Small "alive" LED that pulses with the effect's own envelope gate -
    // a bit of hardware-style feedback that the effect is doing something.
    juce::Point<float> ledCentre;
    float ledLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HyperScapeAudioProcessorEditor)
};
