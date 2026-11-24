# Parameters and schema 6

168 APVTS parameters; 167 stored in each A/B slot, with abMatch global. The first
44 IDs retain historical compatibility. Active shared controls: inputDb/outputDb
(-48..24 dB), bypass, clipEnabled (global ACTIVE), ceilingDb (-24..0 dBFS), mix,
quality (1/2/4/8x), dcFilter, symmetry and editSlot (CURVE 1/2). Legacy mode/routing
and full-band nonlinear IDs no longer control the new shapers; states migrate.

Each prefix master / low / mid / high has these suffixes:

| Suffix | Range | Default |
| --- | --- | --- |
| Enabled | bool | MASTER on, bands off |
| Clip | bool | on |
| Custom | bool | off |
| Drive | 0..36 dB | 0 |
| Threshold | -36..0 dB | 0 |
| Hardness | 0..1 | 0 |
| Knee | 0..1 | 0.2 |
| Curve | Tanh / Arctangent / Cubic / Adjustable knee / Asymmetric | Adjustable knee |
| Bias | -0.5..0.5 | 0 |
| Asymmetry | -1..1 | 0 |
| Morph | 0..1 | 0 |
| Mix | 0..1 | 1 |
| A0..A8, B0..B8 | -1..1 ordinates at fixed x | identity |

bandLowHz: 40..1000 Hz, default 120. bandHighHz: 1200..12000 Hz, default 2500.
bassEnabled defaults off. 808 retains subPreserve (0..1, default 1), harmonics
(0..1, .5), bassDrive (0..36 dB, 0), punch (0..1, 0), splitHz (40..400 Hz, 120),
and bassOutput (-24..12 dB, 0). abMatch defaults off.

Curve points are non-automatable editor controls. Scalar DSP controls are smoothed
and automatable. Double-click knobs for defaults. Viewing a tab is not a parameter.

Schema 6 validates exact IDs, ranges, discrete choices and both comparison slots
before replacing anything. Root and active slot must agree. Schemas 1-5 migrate;
sidechain data is validated then discarded. See WAVESHAPER_WORKFLOW.md for old
808, exclusive routing and automation compatibility limits.
