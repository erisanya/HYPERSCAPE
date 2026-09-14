#include "PluginProcessor.h"
#include "PluginEditor.h"

HyperScapeAudioProcessor::HyperScapeAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      pitchUpA(std::make_unique<HyperPitch>()),
      pitchUpB(std::make_unique<HyperPitch>()),
      pitchDownA(std::make_unique<HyperPitch>()),
      pitchDownB(std::make_unique<HyperPitch>())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout HyperScapeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Single knob: how loud the pitch-shifted effect is, mixed on top of
    // the untouched dry signal (the dry signal never disappears - at 0
    // it's silent, at 100% it's as loud as the effect gets, up to 4x).
    // Kept the param id "amount" for continuity; the knob is labelled MIX.
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "amount", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)); })
            .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue() / 100.0f; })));

    return { p.begin(), p.end() };
}

bool HyperScapeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo())
        && mainOut == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorEditor* HyperScapeAudioProcessor::createEditor()
{
    return new HyperScapeAudioProcessorEditor(*this);
}

void HyperScapeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int curBlockSize = juce::jmax(1, samplesPerBlock);

    // Four decorrelated voices - different grain length and start phase
    // each, so none of them restart a grain at the same instant as another.
    pitchUpA->prepare(currentSampleRate, curBlockSize, 32.0, 0.0);
    pitchUpB->prepare(currentSampleRate, curBlockSize, 39.0, 0.35);
    pitchDownA->prepare(currentSampleRate, curBlockSize, 35.0, 0.6);
    pitchDownB->prepare(currentSampleRate, curBlockSize, 43.0, 0.85);
    pitchUpA->setPitchSemitones(12.0f);
    pitchUpB->setPitchSemitones(12.0f);
    pitchDownA->setPitchSemitones(-12.0f);
    pitchDownB->setPitchSemitones(-12.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(curBlockSize);
    spec.numChannels = 1;

    delayUpA.setMaximumDelayInSamples(static_cast<int>(currentSampleRate * 0.2));
    delayUpB.setMaximumDelayInSamples(static_cast<int>(currentSampleRate * 0.2));
    delayDownA.setMaximumDelayInSamples(static_cast<int>(currentSampleRate * 0.2));
    delayDownB.setMaximumDelayInSamples(static_cast<int>(currentSampleRate * 0.2));
    haasDelay.setMaximumDelayInSamples(static_cast<int>(currentSampleRate * 0.05));

    delayUpA.prepare(spec);
    delayUpB.prepare(spec);
    delayDownA.prepare(spec);
    delayDownB.prepare(spec);
    haasDelay.prepare(spec);
    delayUpA.reset();
    delayUpB.reset();
    delayDownA.reset();
    delayDownB.reset();
    haasDelay.reset();

    auto shelfCoeff = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 5200.0f, 0.7071f, 1.0f);
    toneLeft.coefficients = shelfCoeff;
    toneRight.coefficients = shelfCoeff;
    toneLeft.prepare(spec);
    toneRight.prepare(spec);
    toneLeft.reset();
    toneRight.reset();

    // Mud-control curve for the pitch-shifted bus only: a smooth ~120Hz
    // high-pass (gentle slope, no resonant bump) plus a small dip around
    // 200Hz, matching a low-cut-with-a-dip curve rather than a sharp
    // resonant cut.
    auto mudHPCoeff = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 120.0f, 0.72f);
    mudHPLeft.coefficients = mudHPCoeff;
    mudHPRight.coefficients = mudHPCoeff;
    mudHPLeft.prepare(spec);
    mudHPRight.prepare(spec);
    mudHPLeft.reset();
    mudHPRight.reset();

    auto mudDipCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, 200.0f, 1.1f, juce::Decibels::decibelsToGain(-2.5f));
    mudDipLeft.coefficients = mudDipCoeff;
    mudDipRight.coefficients = mudDipCoeff;
    mudDipLeft.prepare(spec);
    mudDipRight.prepare(spec);
    mudDipLeft.reset();
    mudDipRight.reset();

    // 80Hz split: everything the effect *generates* (pitch layers, the
    // Haas double, the extra width) is fed only from the high side of
    // this filter, so bass frequencies never get pitched or panned off
    // centre. The original low end always passes straight through in the
    // untouched dry "mid" signal below.
    auto hpCoeff = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 80.0f, 0.7071f);
    fxHighpass.coefficients = hpCoeff;
    sideHighpass.coefficients = hpCoeff;
    fxHighpass.prepare(spec);
    sideHighpass.prepare(spec);
    fxHighpass.reset();
    sideHighpass.reset();

    amountSmoothed.reset(currentSampleRate, 0.03);

    auto* rawAmount = parameters.getRawParameterValue("amount");
    const float initialVal = rawAmount != nullptr ? rawAmount->load() : 0.0f;
    amountSmoothed.setCurrentAndTargetValue(initialVal);

    // Faster release than the original (was ~45ms) - the generated layers
    // duck out quickly once the input stops, instead of leaving a tail
    // that reads as "an extra delay" / a repeated syllable at word ends.
    envAttackCoeff  = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.001f));
    envReleaseCoeff = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.02f));
    envelopeState = 0.0f;

    // Oversample just the saturation stage: pushing it to 2x sample rate
    // moves the harmonics/aliasing a nonlinearity creates above Nyquist
    // before they fold back down, which is what made loud passages sound
    // harsh/"weird" before.
    oversampler.initProcessing(static_cast<size_t>(curBlockSize));
    oversampler.reset();
    setLatencySamples(static_cast<int>(oversampler.getLatencyInSamples()));

    wetBuffer.setSize(2, curBlockSize, false, false, true);
}

void HyperScapeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0 || numSamples == 0)
        return;

    if (wetBuffer.getNumSamples() < numSamples)
        wetBuffer.setSize(2, numSamples, false, false, true);

    auto* left = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    auto* wetL = wetBuffer.getWritePointer(0);
    auto* wetR = wetBuffer.getWritePointer(1);

    auto* rawAmount = parameters.getRawParameterValue("amount");
    const float targetAmount = rawAmount != nullptr ? rawAmount->load() : 0.0f;
    amountSmoothed.setTargetValue(targetAmount);

    const float delaySamplesA = 0.009f * static_cast<float>(currentSampleRate);   // ~9 ms
    const float delaySamplesB = 0.0135f * static_cast<float>(currentSampleRate);  // ~13.5 ms
    const float haasSamples   = 0.019f * static_cast<float>(currentSampleRate);   // ~19 ms Haas offset

    constexpr float duplicateLevel = 0.30f;
    constexpr float haasLevel = 0.22f;
    constexpr float widthBoost = 0.55f;
    constexpr float panPosA =  0.70f;
    constexpr float panPosB = -0.70f;
    constexpr float haasPanU = -0.55f;
    constexpr float haasPanD =  0.55f;

    auto panGains = [](float pan, float& gainL, float& gainR)
    {
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
        gainL = std::cos(angle);
        gainR = std::sin(angle);
    };

    float gainLA, gainRA, gainLB, gainRB;
    panGains(panPosA, gainLA, gainRA);
    panGains(panPosB, gainLB, gainRB);

    float gainLU, gainRU, gainLD, gainRD;
    panGains(haasPanU, gainLU, gainRU);
    panGains(haasPanD, gainLD, gainRD);

    float lastGate = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        // Single knob: 0%..100% maps to 0x..4x loudness for the effect
        // bus. The dry mid signal below is always present - this knob
        // never removes it, it only controls how loud the pitch-shifted
        // layer is on top of it.
        const float effectLevel = amountSmoothed.getNextValue() * 4.0f;

        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;

        const float mid  = 0.5f * (inL + inR);
        const float side = 0.5f * (inL - inR);

        // Only content above 80Hz ever gets pitched, duplicated or panned -
        // this is the low end protection so bass never turns muddy.
        const float fxIn   = fxHighpass.processSample(mid);
        const float sideHi = sideHighpass.processSample(side);

        const float rectified = std::abs(fxIn);
        if (rectified > envelopeState)
            envelopeState = envAttackCoeff * envelopeState + (1.0f - envAttackCoeff) * rectified;
        else
            envelopeState = envReleaseCoeff * envelopeState + (1.0f - envReleaseCoeff) * rectified;
        const float gate = std::tanh(envelopeState * 14.0f);
        lastGate = gate;

        // Four voices: +12 st and -12 st, each split into a "pan A" (+70%)
        // and "pan B" (-70%) copy.
        const float shiftedUpA   = pitchUpA->processSample(fxIn);
        const float shiftedUpB   = pitchUpB->processSample(fxIn);
        const float shiftedDownA = pitchDownA->processSample(fxIn);
        const float shiftedDownB = pitchDownB->processSample(fxIn);

        delayUpA.pushSample(0, shiftedUpA);
        delayUpB.pushSample(0, shiftedUpB);
        delayDownA.pushSample(0, shiftedDownA);
        delayDownB.pushSample(0, shiftedDownB);
        const float delayedUpA   = delayUpA.popSample(0, delaySamplesA);
        const float delayedUpB   = delayUpB.popSample(0, delaySamplesB);
        const float delayedDownA = delayDownA.popSample(0, delaySamplesA);
        const float delayedDownB = delayDownB.popSample(0, delaySamplesB);

        // Combine the up+down layer that share a pan position, brighten,
        // then apply the mud-control curve (120Hz soft high-pass + 200Hz
        // dip) so this bus specifically never builds up low-mid mud.
        const float rawA = 0.5f * (delayedUpA + delayedDownA);
        const float rawB = 0.5f * (delayedUpB + delayedDownB);

        const float brightA = toneLeft.processSample(rawA);
        const float brightB = toneRight.processSample(rawB);

        const float mudA = mudDipLeft.processSample(mudHPLeft.processSample(brightA));
        const float mudB = mudDipRight.processSample(mudHPRight.processSample(brightB));

        const float genA = duplicateLevel * gate * mudA;
        const float genB = duplicateLevel * gate * mudB;

        const float genL = gainLA * genA + gainLB * genB;
        const float genR = gainRA * genA + gainRB * genB;

        // Pure (non-pitched) Haas double: an undelayed copy on one side and
        // the same signal ~19ms later on the other, for the "double-tracked"
        // width on top of the octave-up/octave-down layers.
        haasDelay.pushSample(0, fxIn);
        const float haasDelayed = haasDelay.popSample(0, haasSamples);

        const float haasU = haasLevel * gate * fxIn;
        const float haasD = haasLevel * gate * haasDelayed;

        const float haasL = gainLU * haasU + gainLD * haasD;
        const float haasR = gainRU * haasU + gainRD * haasD;

        const float wideSide = widthBoost * sideHi;

        // Additive, not a crossfade: the dry mid is always there, this
        // knob only sets how loud the pitch-shifted/widened layer is on
        // top of it - at 0 you hear only your voice, at 100% the effect
        // is as loud as it gets, but your voice never disappears.
        const float outL = mid + effectLevel * (side + wideSide + genL + haasL);
        const float outR = mid + effectLevel * (genR + haasR - side - wideSide);

        wetL[n] = outL;
        wetR[n] = outR;
    }

    uiActivityLevel.store(lastGate, std::memory_order_relaxed);

    // Run the saturation at 2x sample rate so the harmonics it adds fold
    // back down above what's audible instead of aliasing into harshness.
    auto block = juce::dsp::AudioBlock<float>(wetBuffer).getSubBlock(0, static_cast<size_t>(numSamples));
    auto oversampledBlock = oversampler.processSamplesUp(block);

    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer(ch);
        for (size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
        {
            const float x = data[i] * 1.08f;
            data[i] = std::tanh(x) / std::tanh(1.08f);
        }
    }

    oversampler.processSamplesDown(block);

    for (int n = 0; n < numSamples; ++n)
    {
        left[n] = wetL[n];
        if (right != nullptr)
            right[n] = wetR[n];
    }
}

void HyperScapeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void HyperScapeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HyperScapeAudioProcessor();
}
