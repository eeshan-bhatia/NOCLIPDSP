# Next workflow: focused clipping and waveshaping

Status: design agreed in direction by user; not implemented in 0.5.0. Finish and
validate sidechain/comparison first, then change routing and editor state together.
Existing DSP should be reused rather than replaced.

## Product structure

Two main destinations: WAVESHAPER and 808 ENGINE. Sidechain and whole-plugin A/B
remain shared utilities, alongside oversampling, global input/output, ceiling,
DC filtering and bypass. No general-purpose effects rack or modulation sequencer.

Within WAVESHAPER use a FULL / THREE BAND routing choice and four clearly labelled
edit targets: MASTER, LOW, MID, HIGH. MASTER is the full-band alternative; it is
not an extra serial saturation stage after the bands. LOW/MID/HIGH are independent
shapers running simultaneously in three-band mode, with only one editor visible.
This avoids the earlier wall of repeated controls. Crossover knobs appear only
in three-band mode. Bands must retain independent curves and parameters across
view changes, A/B copies, automation and project recall.

## Reuse and remove redundancy

Unify built-in clip curves and editable curves as one waveshaper function selector.
Retain analytical built-ins so their existing sound is not approximated by nine
points. Custom point editing, symmetry, invert, presets and curve morph reuse the
existing editor/LUT worker. Label curve endpoints distinctly from whole-plugin
A/B (for example CURVE 1 / CURVE 2).

One Drive, Threshold, wet Mix and applicable shape controls per selected shaper.
Show Knee only for the adjustable-knee function. Shape/hardness has no current
effect on custom LUTs: hide it there unless its behavior is deliberately specified
and tested. Bias/asymmetry/morph are presented where they operate, never as inert
knobs. Global Input/Output/Ceiling occur once and are not duplicated per view.

Reuse the tested LR crossover but upgrade its three branches to independent
nonlinear settings and independently published LUTs. Phase-match the summed dry
path and keep stereo linkage, sample ceiling and fixed-latency oversampling.
Parameter IDs and migration must preserve old sessions and automation; hidden
legacy controls may be necessary even when duplicate UI knobs are removed.

808 keeps its clean-sub/dirty-harmonics split, Sub Preserve, Harmonics, Punch and
Split as distinct tools. Expose one effective dirty-branch Drive and one Output
trim in the new workflow. Existing global Drive plus bassDrive, and global Output
plus bassOutput, need migration/legacy handling rather than silently resetting old
projects or adding their dB values beyond the supported range. Keep blue styling.

## Acceptance checks before release

Independent band curves survive host recall and whole-plugin A/B; editing one band
does not alter another. Identity curves reconstruct crossover magnitude/phase.
Dry/wet alignment, crossover automation, quality/routing transitions, aliasing,
sidechain behavior and audio callback allocation checks pass at mono/stereo and
multiple sample rates. Existing projects retain their previous sound. Check the
minimum-size interface for readable labels and compact knobs in all targets.
