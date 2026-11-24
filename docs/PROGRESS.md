# Current release: 0.6.0

Updated 2026-11-24. Product: noclip.DSP, Windows VST3 and standalone.

## Implemented processing

- JUCE/CMake foundation, mono/stereo processing and validated saved state.
- Hard, tanh, arctangent, cubic, adjustable-knee and asymmetric clipping.
- Nine-point bounded curves, symmetry, inversion, curve presets, copy and morph.
- Bias/asymmetry, optional DC filtering and worker-to-audio LUT publication.
- Independent LOW/MID/HIGH processing followed by MASTER and 808.
- Independent CLIP/SHAPE enables and curve pairs for each of the four shapers.
- 808 Sub Preserve, Harmonics, Drive, Punch, Split and Output, with matched
  clean/dirty Linkwitz-Riley splits and stereo-linked attack-sensitive drive.
- Whole-plugin A/B snapshots and attenuation-only RMS matching.
- Shared 1x/2x/4x/8x FIR oversampling, fixed 65-sample latency and aligned dry/wet.
- Preallocated quality switching with warm-up and crossfade.

External sidechain and 808 factory presets have been removed. Schema 6 migrates
previous states; see [workflow](WAVESHAPER_WORKFLOW.md) for compatibility limits.

## Validation

Release VST3 and standalone built on 2026-11-24. All four CTest suites passed;
pluginval strictness level 5 reported SUCCESS. Tests cover DSP, clipping, curves,
state migration, crossovers, comparison, latency and allocation-free processing.

A 9 kHz clipping test measured about 47.4 dB reduction of the selected 21 kHz
alias at 8x versus 1x. See [measurements](PHASE34.md) and [testing](TESTING.md)
for settings, reproduction commands and qualifications.

FL Studio listening and real project reload remain unverified for this build.
No production-mastering, true-peak or LUFS claim is made.

## Artifacts and publishing

- build-native/noclip.DSP_artefacts/Release/Standalone/noclip.DSP.exe
- build-native/noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3

Validated updates are committed and pushed to the project's GitHub repository.
Binaries, SDKs, caches and downloaded dependencies remain ignored.
