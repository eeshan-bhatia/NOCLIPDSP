# Testing and measurements

## Native Windows commands

```powershell
& .tools/python/cmake/data/bin/cmake.exe --build build-native --config Release --parallel 2
& .tools/python/cmake/data/bin/ctest.exe --test-dir build-native -C Release --output-on-failure
./scripts/Validate-Plugin.ps1
py -3.12 scripts/Plot-Analysis.py --build build-native
```

Pinned local environment: JUCE 9.0.5, MSVC 19.52.37029, Windows SDK 10.0.28000.0.
pluginval 1.0.4 uses strictness 5, seed 12345 and host checks. The wrapper
waits for the validator and reports its actual exit code.

## Suites

- DSPTests: gain ramps, bypass, stereo/partition invariance, +24 dB output.
- ClipperTests: built-in curves, bounds, continuity, soft/hard morph, ceiling/mix, sample rates/buffer sizes and standard allocation checks.
- AdvancedTests: bounded curves, audio effects, A/B morph, threaded mailbox integrity, DC rejection, LR reconstruction magnitude/phase, independent band research, harmonics/sub preservation and synthetic pitched/decaying 808 punch.
- StateTests + ProcessingTests + ComparisonTests: parameter/curve recall, invalid state rejection, schema 1/2/3/4 migration, mono/stereo buses, 1x/2x/4x/8x latency and spectra, exact delayed bypass, quality/routing automation and callback allocation checks.

The Phase 3/focused Phase 4 suites pass locally. Reproduce measurements with CTest
rather than treating checked-in evidence as a substitute for running the tests.
Raw validation evidence is retained in [validation](../validation/).
New runs write their logs under build-native.

## Analysis

Tests write four CSV files under build-native. Plot-Analysis.py uses numpy and
matplotlib (optional project-local dependencies in .tools/analysis) to produce
figures under docs/measurements. See PHASE34.md for settings and interpretation.
The chosen 21 kHz alias from a 9 kHz sine falls about 47.4 dB at 8x vs 1x; this is
not a universal alias-rejection claim. Timings are local wall-clock observations.

## 0.6.0

Sidechain tests are replaced with assertions that its bus and controls are absent.
ComparisonTests checks independent band curves, A/B recall/copy, invalid inactive state rejection,
migration, band distortion, custom LUT audio effect, identity with sections off,
and isolated-808 equivalence. Allocation checks include modular quality switching
and A/B.

FL Studio listening and real project reload remain unverified for this build.
