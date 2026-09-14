#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <cmath>

// 4-grain overlap-add pitch shifter using a Hann analysis/synthesis window.
//
// v3 change (fixes audible crackle/"stutter" on pitched material):
// v2 used only 2 overlapping grains with a sine-mapped window. Two grains
// give a window sum that is NOT constant over time - it dips between grain
// centres - so the output amplitude was subtly pumping in time with the
// grain rate, and every grain restart ("jump back and re-read old audio")
// landed on an audible seam with nothing covering it. That reads as fast,
// rhythmic crackle/stutter, especially on transient-heavy material - the
// same character as a granular/transient-stutter effect.
//
// Using 4 grains, each a standard raised-cosine (Hann) window, staggered by
// exactly 1/4 of a grain length, satisfies the classic constant-overlap-add
// (COLA) condition: sum_{k=0..3} Hann(p + k/4) is EXACTLY constant for every
// p. So the combined amplitude of the four crossfading grains never dips or
// bumps, and any single grain's restart seam is covered by three other
// grains that are mid-crossfade (not restarting) at that instant. This is
// the standard fix for this exact artifact in granular time-stretching.
//
// A small amount of random jitter on each grain's restart *read position*
// (not its window timing - that stays perfectly regular, so COLA still
// holds) keeps the restart seams from lining up into a periodic buzz.
class HyperPitch
{
public:
    static constexpr int kNumGrains = 4;

    void prepare(double newSampleRate, int maxBlockSize, double grainLengthMs = 34.0, double startPhaseOffset = 0.0)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        basePhaseOffset = juce::jlimit(0.0, 0.999, startPhaseOffset);

        grainSamples = juce::jmax(64.0, sampleRate * (grainLengthMs * 0.001));
        grainPhaseIncrement = 1.0 / grainSamples;

        bufferSize = static_cast<int>(sampleRate * 0.25) + maxBlockSize + 64;
        buffer.assign(static_cast<size_t>(bufferSize), 0.0f);

        // Per-instance jitter seed so multiple instances fed the same input
        // don't restart grains at identical instants (keeps them
        // decorrelated instead of building a combined comb-filter artifact).
        jitterState = static_cast<uint32_t>(startPhaseOffset * 104729.0) ^ 0x9e3779b9u;
        if (jitterState == 0) jitterState = 0x9e3779b9u;

        reset();
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;

        for (int g = 0; g < kNumGrains; ++g)
        {
            const auto idx = static_cast<size_t>(g);
            const double stagger = static_cast<double>(g) / static_cast<double>(kNumGrains);
            phase[idx] = std::fmod(basePhaseOffset + stagger, 1.0);
            readPos[idx] = grainSamples * phase[idx];
        }
    }

    void setPitchSemitones(float semitones)
    {
        ratio = std::pow(2.0, static_cast<double>(semitones) / 12.0);
    }

    float processSample(float input)
    {
        if (bufferSize <= 0 || buffer.empty())
            return input;

        buffer[static_cast<size_t>(writePos)] = input;

        float output = 0.0f;

        for (int g = 0; g < kNumGrains; ++g)
        {
            const auto idx = static_cast<size_t>(g);
            const float sample = readInterpolated(readPos[idx]);
            const float w = hannWindow(phase[idx]);
            output += sample * w;

            readPos[idx] += ratio;
            phase[idx] += grainPhaseIncrement;

            if (phase[idx] >= 1.0)
            {
                phase[idx] -= 1.0;
                // Up to ~12% of a grain length of jitter on the restart
                // read point only - window timing is untouched.
                const double jitter = (nextJitter() - 0.5) * 0.12 * grainSamples;
                readPos[idx] = wrap(static_cast<double>(writePos) - grainSamples * 0.75 + jitter);
            }
        }

        // 4 Hann windows staggered by 1/4 sum to an exact constant of 2.0 -
        // normalise back to unity gain.
        output *= 0.5f;

        if (++writePos >= bufferSize)
            writePos = 0;

        return output;
    }

private:
    double sampleRate = 44100.0;
    double ratio = 2.0;
    double basePhaseOffset = 0.0;
    int bufferSize = 20000;
    int writePos = 0;

    double grainSamples = 1500.0;
    double grainPhaseIncrement = 1.0 / 1500.0;

    std::array<double, kNumGrains> readPos {};
    std::array<double, kNumGrains> phase {};

    std::vector<float> buffer;

    uint32_t jitterState = 12345;

    double nextJitter()
    {
        // xorshift32 - deterministic, no <random> needed, plenty good
        // enough for a few percent of position jitter.
        jitterState ^= jitterState << 13;
        jitterState ^= jitterState >> 17;
        jitterState ^= jitterState << 5;
        return static_cast<double>(jitterState) / 4294967295.0;
    }

    double wrap(double x) const
    {
        if (bufferSize <= 0) return 0.0;
        x = std::fmod(x, static_cast<double>(bufferSize));
        if (x < 0.0) x += static_cast<double>(bufferSize);
        return x;
    }

    float readInterpolated(double position) const
    {
        if (bufferSize <= 0 || buffer.empty()) return 0.0f;

        position = wrap(position);
        const int i1 = static_cast<int>(position) % bufferSize;
        const float frac = static_cast<float>(position - static_cast<double>(i1));

        const int i0 = (i1 - 1 + bufferSize) % bufferSize;
        const int i2 = (i1 + 1) % bufferSize;
        const int i3 = (i1 + 2) % bufferSize;

        const float y0 = buffer[static_cast<size_t>(i0)];
        const float y1 = buffer[static_cast<size_t>(i1)];
        const float y2 = buffer[static_cast<size_t>(i2)];
        const float y3 = buffer[static_cast<size_t>(i3)];

        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0 + 0.5f * y2;
        const float a3 = y1;

        return ((a0 * frac + a1) * frac + a2) * frac + a3;
    }

    static float hannWindow(double p)
    {
        return static_cast<float>(0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * p));
    }
};
