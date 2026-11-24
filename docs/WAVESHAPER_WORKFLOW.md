# Five-section workflow - 0.6.0

## Navigation and activation

Exactly five tabs: MASTER, LOW, MID, HIGH, 808. Selecting a tab changes
only the view and palette. It never enables, disables or resets processing.
Use each ON switch to activate its section. Multiple sections can run together.
New instances start with MASTER on, its CLIP on, custom SHAPE off, bands/808 off.

LOW/MID/HIGH run in parallel first, MASTER processes their sum, then 808 processes
the result. This is a fixed serial order, not alternative exclusive modes.

## CLIP + SHAPE

Each shaper stores independent settings and two nine-point curve endpoints.
The left side contains an analytical clip transfer preview, Drive, Threshold,
Hardness, Knee and Stage Mix. The right side contains custom curve editing,
CURVE 1/2, presets, symmetry, inversion, COPY, Bias, Asymmetry and Morph.
COPY copies the selected endpoint to the other endpoint in this section.

CLIP and SHAPE have separate switches. When both are on, CLIP precedes SHAPE;
drive is applied once. Threshold scales both functions. When both are off the
section passes through, including its Drive. Stage Mix blends local dry/wet.
Knee operates only for Adjustable knee below full hardness. Bias, Asymmetry and
Morph operate on custom shaping and are disabled in the UI when SHAPE is off.

The crossover knobs are shared by LOW/MID/HIGH. Disabled bands remain in the sum
unchanged. Enabled identity bands have flat crossover magnitude with LR allpass
phase. Disabling the last band crossfades back to raw audio. Filter history stays
warm. Global dry follows active crossover and 808 phase. Wet DC filtering precedes
global mix/output and sample ceiling. Transitions can blend different bounds;
this is not a true-peak limiter.

## 808

The original six tools remain: Sub Preserve, Harmonics, Drive, Punch, Split and
Output. The isolated engine uses the original adjustable-knee clipping response.
Its clean-sub/dirty-harmonics split remains separate from the three-band shapers.
808 has no factory preset menu; adjust its six knobs directly.
Blue styling follows the viewed tab, not whether
808 is processing. Viewing another tab leaves 808 running if its ON switch is on.

## Recall and migration

Schema 6 stores all four shapers, section switches, crossovers and 808 in each
whole-plugin A/B slot. Navigation is editor-local and excluded from DSP snapshots.
Schema 5 slots migrate independently; removed sidechain settings are discarded.
Schemas 1-4 initialize both slots from the old state. Old full-band CLIP/SHAPE
settings and curves map to MASTER. New bands default off. Schema-3 experimental
band routing retains the earlier simplified migration, not a resurrected patch.

Old 808 projects map to enabled 808, but legacy custom-curve/alternative clipping
selection and extra global nonlinear drive/threshold are not reproduced by its
new fixed six-control engine. External-sidechain sound is deliberately removed.
Review these sessions and host automation: new shaper IDs replace the old
exclusive CLIP/SHAPE mode and routing automation. Legacy IDs remain for migration,
not as controls for the new four shapers. Keep the previous build for old sessions
that require its exact processing.

## Validation

Tests cover parameter-invariant navigation, independent low-band curves, A/B
recall, malformed state rejection, migration, all-sections-off identity, isolated
808 equivalence, crossover reconstruction, latency and allocation-free callbacks.
All five views render offscreen at minimum size. Real FL Studio listening and
project reload remain user validation priorities.
