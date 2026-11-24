# noclip.DSP

A C++20/JUCE clipper and waveshaper focused on masters, aggressive drums and 808s.
**0.5.0 is a development build, not a finished mastering limiter.**
Windows VST3 and standalone; original AGPLv3 license preserved.

## Features

- CLIP: hard, tanh, arctangent, cubic, adjustable-knee and asymmetric curves; smoothed drive, threshold, shape, gain, mix and sample ceiling.
- SHAPE: nine-point curve editor, bounded cubic interpolation, symmetry lock, inversion, presets, A/B curve copies and continuous morph.
- 808 ENGINE: sub preservation, harmonics, drive, stereo-linked punch, split frequency and output. Five real factory starting points.
- Shared 1x/2x/4x/8x FIR oversampling; preallocated banks and smooth live quality changes. Fixed 65-sample latency, including 1x and bypass.
- Bias/asymmetry, optional 10 Hz DC blocking, input/output waveform views and readable peak/RMS/activity meters.
- Rotary controls throughout, with a separate GAIN / MIX view in SHAPE and 808.

- SIDECHAIN: external mono/stereo key with sensitivity, drive relief, attack and release. Eases distortion without feeding the key into the output.
- Whole-plugin A/B snapshots, copy in either direction, smooth switching and optional attenuation-only RMS matching. Separate from the curve editor's A/B morph.
- The 808 engine has a slate-blue theme, white text, blue glow and compact knob spacing. Other pages retain sand/green styling.

See [sidechain and comparison usage](docs/SIDECHAIN_AB.md). The next planned
workflow consolidates CLIP/SHAPE and introduces independently editable band
waveshapers; that redesign is **not implemented in 0.5.0**. See
[focused waveshaper design](docs/WAVESHAPER_WORKFLOW.md).

## Build

Install Visual Studio 2022+ Desktop development with C++, a Windows SDK, Git and
CMake 3.22+ (a version supporting your Visual Studio generator).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Use `Visual Studio 18 2026` for VS 2026. JUCE 9.0.5 is pinned at
`d60b7a78030661574a615b586d9325ba7a7fa7dd`; `NOCLIP_JUCE_PATH` selects a local checkout.
`NOCLIP_FONT_FILE` optionally embeds a locally supplied Counter-Strike TTF;
otherwise the UI uses Consolas. The repository does not distribute the font.

Outputs: `build/noclip.DSP_artefacts/Release/Standalone/noclip.DSP.exe` and
`build/noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3`. Close the DAW, replace the
whole VST3 bundle and rescan. Avoid installing both old and current bundles with
the same plugin ID. Native setup and pluginval: [BUILDING](docs/BUILDING.md).

## Validation and limits

Four CTest suites cover clipping, curve publication, state migration, latency,
alias reduction, crossovers, synthetic 808s and allocation-free quality changes.
Pluginval strictness level 5 checks the host-facing plugin.

![Oversampling comparison](docs/measurements/oversampling-spectrum.png)

In the documented 9 kHz clipping test at 48 kHz, a selected 21 kHz alias drops
about 47 dB from 1x to 8x. This is one controlled test, not a universal specification.
See [Phase 3/4 design and measurements](docs/PHASE34.md).

The ceiling is a sample clamp, not a true-peak limiter. A final sample guard catches
reconstruction overshoot but can itself introduce aliasing. Sub Preserve protects
a filtered low branch; it does not guarantee exact fundamental amplitude, and
output gain/ceiling still affect it. No LUFS, true-peak or dynamic limiter is claimed. RMS matching is an
approximation, not perceptual loudness normalization. FL Studio listening/project recall is not yet
validated by this build's automated tests.

Schema 4 saves all current controls and both curves. Earlier states migrate
conservatively. Output now spans -48..+24 dB; review Output automation from older
-48..+12 dB builds because normalized mappings changed.

[Progress](docs/PROGRESS.md) | [Architecture](docs/ARCHITECTURE.md) |
[Parameters](docs/PARAMETERS.md) | [UI](docs/UI.md) | [Testing](docs/TESTING.md)

## License

[AGPLv3](LICENSE). JUCE is dual licensed under AGPLv3 or commercial terms:
[JUCE license](https://github.com/juce-framework/JUCE/blob/9.0.5/LICENSE.md),
[third-party inventory](https://github.com/juce-framework/JUCE/blob/9.0.5/JUCE.spdx.json).
Distribution outside AGPLv3 terms may require a commercial JUCE license. Confirm
permission before distributing a build containing the externally supplied font.
Dependencies, SDKs, caches and compiled binaries are ignored.
