This file records the original 1x engine. For current oversampling, DC filtering, routing and latency, see PHASE34.md and ARCHITECTURE.md.

# Phase 2 clipping reference (0.2.0)

Let T = 10^(thresholdDb/20), u = x/T and s = Shape. The output of a selected
soft family f is morphed with conventional hard clipping:

y = (1-s)*T*f(u) + s*clamp(x,-T,T).

All families have unit small-signal slope and bounded magnitude <= T. Positive
and negative halves are odd-symmetric except the explicitly asymmetric family.
Double intermediates prevent finite float inputs overflowing during drive or
normalization. Nonfinite audio is replaced by zero; invalid controls use defaults.

## Transfer functions

- Hard: clamp(x,-T,T).
- Tanh: f(u)=tanh(u).
- Arctangent: f(u)=(2/pi)*atan((pi/2)*u).
- Cubic: v=clamp(u,-1.5,1.5); f(u)=v-(4/27)*v^3.
- Asymmetric: cubic(u) for u>=0; 0.65*cubic(u/0.65) for u<0.
  This generates even as well as odd harmonics and can create DC. It is not
  independent per-harmonic control. DC filtering is not implemented yet.
- Adjustable knee: k=Knee, a=abs(u). For k=0 use hard clipping. For a<=1-k,
  f(u)=u; for a>=1+k, f(u)=sign(u). In the transition, d=a-(1-k) and
  f(u)=sign(u)*(1-k+d-d^2/(4*k)). Value and first derivative match at both joins.
  For k<1e-6 the implementation uses the hard limit to avoid division instability.

The default family is Adjustable knee with k=0.2 and T=1. Samples below 0.8
are exactly linear at initial settings. Raising Drive moves audio into the knee.
Different curve families crossfade with normalized nonnegative smoothed weights;
rapid interrupted switches remain bounded.

## Routing and bounds

dry = input * inputGain; driven = dry * driveGain;
wet = transfer(driven); mixed = (1-Mix)*dry + Mix*wet;
processed = clamp(mixed*outputGain, -Ceiling, Ceiling).

Clip enable crossfades processed with dry*outputGain. Global bypass crossfades
that result with original input. Fully disabled clip/global bypass skips nonlinear
evaluation. No hidden gain normalization is applied.

Ceiling is a sample-peak hard clamp after the blend, not a lookahead dynamic
limiter or a reconstructed true-peak bound. Even Mix=0 retains the active ceiling.
Crossfades to/from bypass/disabled processing deliberately release that bound.
Automated ceiling values ramp for 20 ms.

## Real-time and measurements

Finite-duration linear ramps advance once per frame so stereo control trajectories
match. Curves are memoryless; channels share controls, not an envelope detector.
No dynamic storage, filesystem, locks or GUI access occurs in engine processing.
Latency is zero; dry and wet are naturally aligned.

Input/output peak is max(abs(x)). RMS uses double-precision energy accumulation
over both channels and the latest block. Clipping activity is the proportion of
samples whose driven magnitude exceeds T or whose pre-ceiling magnitude exceeds
Ceiling, weighted by enable/bypass transitions. It is an activity indicator, not
gain reduction; soft saturation can begin below T. UI sampling can miss short events.

Nonlinear functions generate harmonics; harmonics above Nyquist fold into the
audible spectrum at the current 1x rate. No anti-aliasing claim is made.
Oversampling and its latency handling belong to Phase 3. Clipping bounds samples
instantaneously; a dynamic limiter instead detects peaks/envelopes and applies
time-varying gain, usually with lookahead. The latter is not implemented here.
