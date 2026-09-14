#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "HyperPitch.h"

class HyperScapeAudioProcessor : public juce::AudioProcessor
{
public:
    HyperScapeAudioProcessor();
    ~HyperScapeAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HYPER SCAPE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.35; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Current effect activity (0-1, the same envelope gate the pitch/haas
    // layers duck with), for the editor's activity LED. Cheap: written
    // once per audio block, read from the message thread at ~30Hz.
    float getActivityLevel() const { return uiActivityLevel.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState parameters;

private:

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Four decorrelated pitch-shifted voices: two octave-up (+12 st), two
    // octave-down (-12 st), each hard-panned to +-70% so the pair reads as
    // a wide stereo doubler instead of a mono effect. Different grain
    // length + start phase per voice keeps them from all restarting a
    // grain at the same instant.
    std::unique_ptr<HyperPitch> pitchUpA;   // +12 st, panned right
    std::unique_ptr<HyperPitch> pitchUpB;   // +12 st, panned left
    std::unique_ptr<HyperPitch> pitchDownA; // -12 st, panned right
    std::unique_ptr<HyperPitch> pitchDownB; // -12 st, panned left

    juce::dsp::DelayLine<float> delayUpA    { 192000 };
    juce::dsp::DelayLine<float> delayUpB    { 192000 };
    juce::dsp::DelayLine<float> delayDownA  { 192000 };
    juce::dsp::DelayLine<float> delayDownB  { 192000 };
    juce::dsp::DelayLine<float> haasDelay   { 48000 }; // pure (non-pitched) Haas double

    juce::dsp::IIR::Filter<float> toneLeft;
    juce::dsp::IIR::Filter<float> toneRight;

    // "Mud control" EQ applied only to the combined pitch-shifted layers
    // (never the dry mid/side signal): a smooth ~120Hz high-pass slope plus
    // a small dip around 200Hz, matching the low-cut + dip curve requested,
    // so the shifted layers stop stacking low-mid mud without turning thin
    // or "tinfoil".
    juce::dsp::IIR::Filter<float> mudHPLeft, mudHPRight;
    juce::dsp::IIR::Filter<float> mudDipLeft, mudDipRight;

    // Keeps everything the effect *generates* (pitch layers, Haas double,
    // extra width) out of the sub-80Hz range, so low end never gets
    // pitched up or panned off-centre and turns muddy.
    juce::dsp::IIR::Filter<float> fxHighpass;
    juce::dsp::IIR::Filter<float> sideHighpass;

    // Oversampled soft saturation stage: running the nonlinearity at 2x
    // sample rate pushes the harmonics/aliasing it creates above Nyquist
    // before they fold back down, which is what was making loud passages
    // sound harsh/"weird".
    juce::dsp::Oversampling<float> oversampler { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::AudioBuffer<float> wetBuffer;

    // Single knob: how loud the effect bus is, mixed additively on top of
    // the always-present dry mid signal.
    juce::SmoothedValue<float> amountSmoothed;

    std::atomic<float> uiActivityLevel { 0.0f };

    float envelopeState = 0.0f;
    float envAttackCoeff = 0.0f;
    float envReleaseCoeff = 0.0f;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HyperScapeAudioProcessor)
};
