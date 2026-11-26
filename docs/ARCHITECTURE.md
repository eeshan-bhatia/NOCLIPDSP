# Architecture - 0.7.0

The plugin enables ModularDSP within AudioEngine's common oversampled domain.
It reuses clip(), CurveTable interpolation, AdvancedDSP's curve transitions and
808 engine, and the tested LR crossover. The old AudioEngine analytical path
remains for core regression tests.

A three-band split feeds independent LOW/MID/HIGH processors. Their sum feeds
MASTER, then 808. Disabled bands pass through. Each Stage has smoothed enable,
CLIP, SHAPE, Drive, Threshold, Hardness, Knee, curve weights and Mix. CLIP precedes
SHAPE. Four independent LUT pairs retain custom curves. Bias/Asymmetry/Morph
reuse AdvancedDSP. Global dry follows active crossover/808 phase. Wet DC filtering
precedes global mix/output and ceiling.

ProcessingChain preallocates ten banks (five oversampling ratios, with/without ADAA) and pads them to 68 host samples.
Quality changes warm a bank then crossfade; each receives all latest stage tables.
ModularDSP allocates on first prepare, never during processing/quality changes.
No external sidechain bus, detector or controls remain.

ComparisonEngine owns two chains; normally the audible slot runs, with both during
switches or RMS matching. See COMPARISON.md for timing and limits.

The worker builds four LUT pairs for each A/B slot. Eight SPSC mailboxes publish
completed curves without audio locks. Pending tables are retained until their
points match the audio snapshot. Prepare synchronously installs all tables.
Audio attempts one bounded parameter read and retains the previous snapshot on
transaction overlap. State transactions use a mutex outside the audio callback.

Schema 7 stores both complete configurations. Legacy IDs remain for
validation and migration.
