# HYPER SCAPE

<img width="250" height="340" alt="image" src="https://github.com/user-attachments/assets/c87e9184-3e38-44a1-8931-ac9b3b570ed1" />

**Vocal stereo widening / octave-up hyper layer**

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
- CMake 3.22+
- Visual Studio 2022 with Desktop development with C++
- Git
- Internet access for the first configure/build because JUCE is fetched automatically

From this folder:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The VST3 and Standalone targets are copied after build.

Typical output:
`build/HYPER_SCAPE_artefacts/Release/VST3/HYPER SCAPE.vst3`

## FL Studio

Copy/install the VST3 into a folder scanned by FL Studio, or let the build's copied VST3 be available in your plugin path.

Then:
1. FL Studio -> Options -> Manage plugins
2. Verify the VST3 search path
3. Find `HYPER SCAPE`
4. Put it on a vocal insert
5. Start low and turn HYPER upward

## Important DSP note

The octave-up engine here is a compact, self-contained grain shifter designed to avoid extra dependencies. It is not intended to compete with a commercial phase-vocoder/pitch-shifter for artifact-free +12 semitone processing. The surrounding widening/saturation design is deliberately musical.

## Suggested product identity

Name: **HYPER SCAPE**
Tagline: **VOCAL STEREO // HYPER WIDTH**

Visual direction:
- dark purple analog rack chassis
- violet backlit controls
- vintage screws and panel lines
- one big hero knob
- tiny OUTPUT trim
