# HYPER SCAPE

<img width="250" height="340" alt="image" src="https://github.com/user-attachments/assets/da1892f2-9a7e-448e-b2e8-f71351d2d7f4" />

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

## Requirements

- CMake (3.22+)
- Git (needed so CMake can fetch JUCE automatically)
- Visual Studio Community (with the "Desktop development with C++" workload)

## Building on Windows

1. Unzip this project.
2. Open the **Developer PowerShell for VS** (Start menu → your Visual Studio version)
3. Run:

```powershell
cd C:\*YOUR-PATH*
cmake -B build
cmake --build build --config Release
```

The first build will take a while — CMake's `FetchContent` downloads JUCE itself
the first time. After that, rebuilds are much faster.

## Output location

After a successful build:

- VST3: `build\HYPER_SCAPE_artefacts\Release\VST3\HYPER SCAPE.vst3`
- Standalone app: `build\HYPER_SCAPE_artefacts\Release\Standalone\HYPER SCAPE.exe`

Copy the `.vst3` into your DAW's VST3 folder (usually
`C:\Program Files\Common Files\VST3`) if it isn't picked up automatically —
`COPY_PLUGIN_AFTER_BUILD` is already set in `CMakeLists.txt` so this normally
happens for you.
