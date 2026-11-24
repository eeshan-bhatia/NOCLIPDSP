# noclip.DSP

A C++20/JUCE clipper and waveshaper for masters, aggressive drums and 808s.
**0.6.0 is a development build.** Windows VST3 and standalone; AGPLv3.

## Five independent sections

MASTER, LOW, MID, HIGH and 808. Tabs only change the view. Each section
has its own ON switch, and multiple sections can run together.

Signal order: input -> LOW/MID/HIGH in parallel -> MASTER -> 808 -> wet DC filter
-> global mix/output -> sample ceiling. Disabled bands pass their range through.
When all three bands are off, the crossover leaves the audible path.

Each of the four shaper sections has CLIP on the left and SHAPE on the right.
Enable either or both; clipping precedes custom shaping. Drive, Threshold and
Stage Mix are shared. Built-in curve families, hardness, knee, nine editable
points, symmetry, inversion, presets, two curve endpoints and morph are retained
independently for every section.

808 keeps Sub Preserve, Harmonics, Drive, Punch, Split and Output, plus its blue
palette and compact knobs. Oversampling, whole-plugin A/B and attenuation-only
RMS matching remain shared. **External sidechain has been removed.**

See [workflow and state notes](docs/WAVESHAPER_WORKFLOW.md),
[comparison guide](docs/COMPARISON.md) and [parameters](docs/PARAMETERS.md).

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

Four CTest suites cover clipping, curve publication, state migration, independent
band shaping, navigation/theme invariance, latency, alias reduction, crossovers,
808 equivalence and allocation-free processing/quality changes.
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

Schema 6 saves every section and both whole-plugin snapshots. Earlier states
migrate with the compatibility limits described in WAVESHAPER_WORKFLOW.md. Output now spans -48..+24 dB; review Output automation from older
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
