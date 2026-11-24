# Implementation progress

Updated 2026-11-24. Product: noclip.DSP 0.4.0, Windows VST3 and standalone.

## Phases 1 and 2 â€” implemented

Buildable JUCE/CMake foundation; mono/stereo; APVTS state; input/output/bypass;
hard, tanh, arctangent, cubic, knee and asymmetric clipping; smoothed controls;
ceiling/mix; measured meters; original AGPLv3 license preserved.

## Phase 3 â€” implemented, native validation

- Nine-point bounded curve editor; symmetry/half-wave editing and inversion.
- Linear/Soft/Hard/Fold curve presets; persistent A/B curves, copy and morph.
- Bias/asymmetry and optional DC filter; safe worker-to-audio LUT publication.
- Shared 1x/2x/4x/8x FIR oversampling, fixed host delay and dry/wet alignment.
- Preallocated live quality switching, with warm-up and crossfade.
- Actual input/output waveform snapshots; all visible continuous controls use knobs.
- Reproducible alias spectrum, CPU measurements, latency and allocation tests.

## Phase 4 â€” revised user scope implemented

- 808 Engine with Sub Preserve, Harmonics, Drive, Punch, Split and Output.
- Stereo-linked attack-sensitive drive reduction; matched clean/dirty LR splits.
- Five parameter presets and a separate GAIN / MIX view for common controls.
- Synthetic 808, harmonic generation, sub-preservation and phase/magnitude tests.
- Full multiband UI and host controls removed at the user's request. The tested
  prototype remains internal research code, not an advertised plugin feature.

## Current UI / state

Original #013220 green fills with stronger glow, static sand texture, supplied
Counter-Strike font (external optional build input), existing scope logo retained.
High-quality scaling and vector reinforcement preserve its upper vertical stroke.
UI title NOCLIP.DSP; product and output filenames noclip.DSP. Schema 4 preserves
current controls and curves; schemas 1/2 and the development schema 3 migrate.

## Validation

Four CTest suites cover DSP, clipping, advanced processing and state/oversampling.
Measured linear impulse latency is 65 samples at every quality. A 9 kHz clipping
test shows about 47.4 dB reduction of the selected 21 kHz alias at 8x versus 1x.
Native UI checks include curve dragging, page switching, knob layout and logo.
See PHASE34.md and TESTING.md for commands, measurements and qualifications.

FL Studio listening, project save/reload and complete accessibility remain user
validation priorities. No production-mastering, true-peak or LUFS claim is made.

## 0.5.0 — sidechain and whole-plugin A/B

Implemented optional mono/stereo external sidechain, four rotary controls and
host bus isolation. Relief eases nonlinear drive to unity without ducking clean
808 branches. Complete A/B snapshots include curves, quality and sidechain;
copy, host recall, warm switching and optional attenuation-only RMS matching.
Schema 5 validates both banks and migrates schemas 1–4. Audio callback allocation
checks now cover matching, sidechain and snapshot switches.

808 uses slate blue (#788e9d background, darker blue surfaces), white text and
blue glow, with compact centered knobs. No 808 DSP controls were removed in this
update. Other pages retain sand/green styling. Theme follows the active routing,
including while the sidechain utility page is open.

## Next: focused waveshaper workflow

The user's latest direction supersedes the earlier general multiband deferral:
consolidate CLIP/SHAPE and introduce MASTER / LOW / MID / HIGH editing with
independent band curves in three-band mode. Keep 808 as the distinct flagship.
This is a subsequent routing/state/UI change, not implemented in 0.5.0; see
WAVESHAPER_WORKFLOW.md for reuse, control deduplication and migration requirements.

## Artifacts and publishing

- build-native/noclip.DSP_artefacts/Release/Standalone/noclip.DSP.exe
- build-native/noclip.DSP_artefacts/Release/VST3/noclip.DSP.vst3

User requested validated updates be committed and pushed to the project's GitHub
repository. Source, tests, docs and original artwork are included; binaries, SDKs,
external font and downloaded dependencies remain ignored.

Final 0.4.0 validation on 2026-11-24: Release VST3/standalone built, all 4 CTest
suites passed, pluginval level 5 SUCCESS. Mirrored curve dragging and the 808 knob
page were visually checked. User stopped further Computer Use; no further UI
control was performed. Final small font/control-state refinements were compiled
and plugin-validated. Phase 3/808 FL Studio listening/recall remains unverified.

0.5.0 validation: Release VST3/standalone built; all four CTest suites and pluginval
level 5 passed. Offscreen minimum-size editor renders checked both palettes and
sidechain controls without opening audio devices. Real FL Studio sidechain routing,
listening and project recall remain user validation priorities.
