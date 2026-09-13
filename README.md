# HYPER SCAPE

<img width="250" height="340" alt="image" src="https://github.com/user-attachments/assets/d3b72759-68a3-4021-b36f-01ee40b1ded4" />


Vocal stereo widening / octave-up hyper layer

HYPER SCAPE is a one-knob-inspired vocal widening effect. The main HYPER control blends in two generated +12-semitone voices with asymmetric Haas-style offsets, stereo separation, a gentle high-frequency lift, and soft saturation.

## What the HYPER knob does

At 0%:
- essentially dry/centered signal

As you turn it up:
- generates octave-up vocal layers
- offsets the two layers by different short delays
- places the generated layers on opposite sides
- keeps the original vocal present
- adds a subtle high-shelf lift to the generated layer
- adds gentle saturation for density

This is intentionally more "effect" than a clinical utility widener.

## Build

Requirements:
- CMake 3.32+ (needed for the Visual Studio 2026 generator)
- Visual Studio 2026 with Desktop development with C++
- Git
- Internet access for the first configure/build because JUCE is fetched automatically

From this folder, in Developer PowerShell for VS 2026:

cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
(Visual Studio 2026 is internally versioned "18" — that's why the generator string says 18, not 2026. If you're on an older CMake without 2026 support, or just want CMake to auto-detect whichever VS you have installed, you can also omit the -G flag entirely: cmake -B build.)

The VST3 and Standalone targets are copied after build.

Typical output:
build/HYPER_SCAPE_artefacts/Release/VST3/HYPER SCAPE.vst3

## FL Studio

Copy/install the VST3 into a folder scanned by FL Studio, or let the build's copied VST3 be available in your plugin path.

Then:
1. FL Studio -> Options -> Manage plugins
2. Verify the VST3 search path
3. Find HYPER SCAPE
4. Put it on a vocal insert
5. Start low and turn HYPER upward

## Important DSP note

The octave-up engine here is a compact, self-contained grain shifter designed to avoid extra dependencies. It is not intended to compete with a commercial phase-vocoder/pitch-shifter for artifact-free +12 semitone processing. The surrounding widening/saturation design is deliberately musical.

## Suggested product identity

Name: HYPER SCAPE
Tagline: VOCAL STEREO // HYPER WIDTH

Visual direction:
- dark purple analog rack chassis
- violet backlit controls
- vintage screws and panel lines
- one hero knob (HYPER, shown as a 0-100% readout), no other controls
