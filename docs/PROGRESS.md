# Current development build: 0.7.0

Updated 2026-11-26. Product: noclip.DSP, Windows and macOS VST3.

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
- Shared 1x/2x/4x/8x/16x FIR oversampling, fixed 68-sample latency and aligned dry/wet.
- Preallocated quality switching with warm-up and crossfade.

External sidechain and 808 factory presets have been removed. Schema 7 migrates
previous states; see [workflow](WAVESHAPER_WORKFLOW.md) for compatibility limits.

- Optional first-order ADAA for built-in clips, custom curves and 808 distortion.

## Validation

0.7.0 Windows Release VST3 and standalone built on 2026-11-26. All five CTest suites passed;
pluginval strictness level 5 reported SUCCESS. Tests cover DSP, clipping, curves,
state migration, crossovers, comparison, latency and allocation-free processing.

The previous 0.6.0 macOS universal prerelease was published on 2026-11-25. Both arm64 and x86_64 builds
passed all four suites and pluginval strictness level 5, with Intel execution
under Rosetta on macOS 15. The extracted ZIP passed signature verification.
Saved-state numeric parsing was made portable to Apple's C++ library; continuous
parameter tests allow tight rounding tolerances. No DSP algorithms were changed.

The Mac package targets macOS 11+, but older systems, physical Intel hardware and
real Mac DAW sessions remain unverified. It is ad-hoc signed, not Developer ID
signed or notarized. See [macOS prerelease](https://github.com/eeshan-bhatia/NOCLIPDSP/releases/tag/v0.6.0-macos).

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
