# Parameters and saved state (schema 5)

| ID | Range / choices | Default |
| --- | --- | --- |
| inputDb, outputDb | -48..+24 dB | 0 |
| bypass | off/on | off |
| clipEnabled | ACTIVE off/on | on |
| curve | Tanh, Arctangent, Cubic, Adjustable knee, Asymmetric | Adjustable knee |
| driveDb | 0..36 dB | 0 |
| thresholdDb | -36..0 dB | 0 |
| shape | soft-to-hard 0..1 | 0 |
| knee | 0..1 | 0.2 |
| ceilingDb | -24..0 dBFS | 0 |
| mix | 0..1 | 1 |
| mode | CLIP / SHAPE | CLIP |
| routing | FULL BAND / 808 ENGINE | FULL BAND |
| quality | 1x / 2x / 4x / 8x | 4x |
| dcFilter | off/on | on |
| bias | -0.5..0.5 | 0 |
| asymmetry | -1..1 | 0 |
| curveMorph | A=0..B=1 | 0 |
| subPreserve | 0..1 | 1 |
| harmonics | 0..1 | 0.5 |
| bassDrive | 0..36 dB | 0 |
| punch | 0..1 | 0 |
| splitHz | 40..400 Hz | 120 |
| bassOutput | -24..12 dB | 0 |
| curveA0..curveA8, curveB0..curveB8 | -1..1 ordinates at fixed x positions | identity |
| symmetry | lock mirrored editing off/on | on |
| editSlot | A / B | A |

| scEnabled | external key off/on | off |
| scSensitivity | -60..0 dBFS | -24 |
| scAmount | 0..24 dB drive relief | 6 |
| scAttack | 0.1..100 ms | 5 |
| scRelease | 10..1000 ms | 150 |
| abMatch | RMS matching off/on | off |

There are 50 live parameters. The 49 except abMatch are stored in each whole-plugin
snapshot. A/B selection is a UI command, saved as the COMPARE active property;
MATCH is global and remains unchanged when switching. The existing curve A/B
parameters retain their separate meaning.

 Curve points are non-automatable editor controls;
other DSP controls (including morph) support host automation. Point edits publish
via a worker and crossfade. Values, routing and quality changes are smoothed.
Double-click knobs for defaults. Numeric boxes accept ASCII and Unicode minus.

CLIP/SHAPE chooses the nonlinear function; 808 routing can use either. Shared
Input, Drive, Threshold, Output, Ceiling and Mix remain accessible through GAIN /
MIX in the advanced pages. 808 Drive adds drive only in its dirty branch; 808 Output
is before global mix/output. ACTIVE off retains input/output gain and removes the
nonlinear chain/ceiling. BYPASS returns the latency-aligned original signal.

The sample ceiling applies after mix/output when active. Enable/bypass transitions
can blend uncapped paths, so no instantaneous bound is promised during transitions.
No true-peak guarantee. See PHASE34.md for filters, latency and sub-preservation.

Schema 5 validates root and both snapshot IDs, finite values, ranges and discrete choices before
replacement. Schema 1's three parameters migrate with clipping disabled. Schema 2's
11 parameters retain values; new curve/808 processing is off, quality=1x, DC=off.
Development schema 3 retains curves/808 settings, maps routing to the simplified
choices and removes its 17 band-control parameters. Invalid states are rejected.
Plugin IDs and NOCLIPDSP_STATE root remain unchanged.

Output was extended from -48..+12 to -48..+24 dB before this release. Saved dB values
remain intact, but existing normalized Output automation needs review.

Schema 5 stores two SLOT children inside COMPARE, each with 49 parameter values.
Active slot and root values must agree. Validation completes before replacing any
state. Schema 4 clones its existing sound into both slots with sidechain and MATCH
off. Schemas 1–3 retain their previous migration behavior and initialize both slots.
