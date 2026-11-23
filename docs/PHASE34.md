# Phase 3 and focused Phase 4 (0.4.0)

The user revised the scope to a master/808 clipper. General multiband controls were
removed after their prototype. Current pages are CLIP, SHAPE and 808 ENGINE.
Sidechain and level-matched whole-plugin A/B are next, not part of this update.

## Waveshaping

Each curve uses nine fixed x positions (-1..1) with y ordinates bounded to [-1,1].
Adjacent slopes of opposite sign receive zero tangents; otherwise their harmonic
mean sets the interior tangent. Cubic Hermite segments are clamped to endpoint
bounds to prevent unintended overshoot, including non-monotonic folds. The audio
engine interpolates a 2049-entry LUT and holds its endpoint outside the domain.

Symmetry lock mirrors edits with opposite polarity and anchors the center at zero.
Unlocking permits independent positive/negative editing. Presets: Linear, Soft,
Hard and Fold. Inversion, A-to-B/B-to-A copies and continuous A/B morph are provided.
A/B mixes transfer output, not loudness; it is not whole-plugin snapshot storage.

In SHAPE mode, Bias shifts input by up to half the threshold. Asymmetry applies opposite drive
changes to the two polarities. These change harmonic character, not independent
harmonic levels. The optional 10 Hz DC blocker operates on wet output before mix:

    r = exp(-2*pi*10/sampleRate)
    y[n] = (1+r)/2 * (x[n]-x[n-1]) + r*y[n-1]

Its enable blend is smoothed. New instances enable it; migrated old sessions do not.
It affects very low frequencies and does not remove DC from entirely dry input.

## 808 engine

Clean and distorted full-band signals enter separate fourth-order Linkwitz-Riley
splits at 40..400 Hz. Generated upper harmonics therefore exist even for a low sine.

    low = SubPreserve*cleanLow + (1-SubPreserve)*dirtyLow
    high = (1-Harmonics)*cleanHigh + Harmonics*dirtyHigh
    wet = (low+high)*808OutputGain

The dirty source uses global Drive and 808 Drive, then the selected CLIP or SHAPE
transfer. Global Input precedes both branches. Optional DC filtering, phase-matched
wet/dry mix, global Output and sample Ceiling follow recombination.

Preserving a low branch is not preserving exact fundamental amplitude: crossover
skirts overlap, nonlinear content crosses the split, and global gain/ceiling still
apply. At SubPreserve=1 and Harmonics=0, the pre-DC/pre-ceiling response has unity
magnitude with allpass phase. The parallel dry signal uses this same phase response.

Punch uses 1 ms and 30 ms amplitude envelopes of the stereo maximum input:

    attack = clamp((fast-slow)/(fast+1e-9),0,1)
    effective808Drive = 1+(requested808Drive-1)*(1-Punch*attack)

It eases distortion on attacks and never exceeds requested drive. It is not a
lookahead limiter or the later general transient processor. Presets are actual
configurations: 808 DESTROY, SUB CONTROL, HARMONIC BOOST, CLEAN 808 and DIGITAL BASS.
They select 4x, DC filtering and a -1 dBFS sample ceiling. CLEAN 808 still includes
the allpass phase, DC filter and ceiling; it is not bit-transparent bypass.

## Oversampling

1x Creative, 2x Eco, 4x Normal and 8x High use JUCE 9.0.5 FIR equiripple filters at
maximum quality and integer latency. New instances default to 4x; old sessions to
1x. All modes, including bypass, use 65 samples of fixed latency (about 1.35 ms at
48 kHz). Linear impulse-peak and exact bypass-delay tests verify reporting.

The replacement bank is already allocated, warmed for 100 ms and crossfaded over
20 ms. Changes arriving mid-transition are applied afterward. Standard new/new[]
instrumentation verifies zero allocations during callback processing/quality
changes; code inspection covers the absence of lock/I/O/allocation APIs. This is
not a claim that instrumentation sees every third-party allocation mechanism.

Downsampling can overshoot. A final host-rate sample clamp catches those peaks and
can itself add aliasing when active. No true-peak ceiling or alias-free guarantee
is made. The 1x mode intentionally remains available for alias-prone character.

## Measurements

Release CTest writes oversampling-spectrum.csv, oversampling-cpu.csv,
crossover-response.csv and 808-harmonics.csv into the build directory. Generate
figures with `py -3.12 scripts/Plot-Analysis.py --build build-native` using numpy
and matplotlib; ignored project-local .tools/analysis dependencies are supported.

The alias test uses a 0.5-amplitude 9 kHz sine at 48 kHz, +12 dB drive, -12 dB
threshold and hard clipping. After 8192 settling samples, an 8192-point rectangular
FFT analyzes the coherent tone. The selected 21 kHz folded harmonic measured:

| Quality | Alias amplitude | Reduction versus 1x |
| --- | ---: | ---: |
| 1x | 0.0939826 | 0 dB |
| 2x | 0.0095209 | 19.9 dB |
| 4x | 0.00492981 | 25.6 dB |
| 8x | 0.000400814 | 47.4 dB |

This is one test signal and parameter set, not a universal attenuation specification.
Timing CSVs measure a stereo 256-frame block including test input generation;
they are local wall-clock observations, not DAW CPU percentages or worst-case bounds.

Tests cover DC rejection, curve bounds/morph/audio changes, untorn threaded table
publication, unity crossover magnitude/phase at 44.1/48/96/192 kHz, independent band
research, 50 Hz harmonic/sub-preservation response, a synthetic decaying/pitched
808, bounded quality/routing changes and saved-state migration. No external audio
samples are required. Prototype three-band recombination matches two cascaded
allpasses; it is retained for research/testing but not exposed as a product feature.

## Limits

- Curve ordinates update via worker publication, with roughly 10 ms polling and
  20 ms crossfade. They are not sample-accurate automation targets.
- Waveforms show approximate latest blocks. Peak/RMS/activity are not gain reduction,
  integrated loudness or true-peak meters. Soft shaping can occur below the activity
  threshold, so activity is not a measure of all nonlinear change.
- No external sidechain, matched full-parameter A/B, LUFS or dynamic limiter yet.
- Native automated validation is not a completed FL Studio listening/recall test.
