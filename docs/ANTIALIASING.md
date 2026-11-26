# Anti-aliasing in 0.7.0

Oversampling supports 1x, 2x, 4x, 8x and 16x. The default remains 4x.
16x costs more CPU and is most useful when strong distortion makes its audible
benefit worth the additional processing. Oversampling alone remains available.

Optional first-order antiderivative anti-aliasing (ADAA) evaluates the average
transfer over consecutive input samples: `(F(x)-F(previous))/(x-previous)`.
Nearly equal inputs use the transfer at their midpoint to avoid cancellation.
Analytical primitives cover every built-in clip family. Custom curves use the
exact integral of their interpolated lookup table, including saturated tails.
Bias, asymmetry, morph and curve transitions share the same current coefficients
for both antiderivative evaluations. The method follows
[Parker, Zavalishin and Le Bivic, DAFx 2016](https://www.dafx.de/paper-archive/details/vem_XXF5qBbfiWOH2RVVAA).

ADAA applies to the four sections' CLIP and SHAPE transfers and the distorted
808 branch. The clean 808 branch and disabled sections retain pure delay paths.
First-order ADAA inherently averages between samples; at 1x this can soften
high frequencies. Use 4x or higher with ADAA as a starting point, and compare
with ADAA off at matched loudness. ADAA is not an alias-free guarantee.
The final sample ceiling remains a direct clamp and can generate aliasing.

Reported plugin latency is fixed at 68 host samples across all ten combinations.
Each serial stage reserves one oversampled sample for ADAA alignment. Additional
padding completes a whole host sample before downsampling; host-rate fractional
interpolation is avoided. Quality/ADAA changes warm the replacement bank for
100 ms and crossfade over 20 ms. Both A/B snapshots retain their own settings.
No bank or antiderivative table is allocated during audio processing.

ADAA defaults off, including migrated projects. Schema 7 stores the new setting;
old quality indices retain their meaning. Adding 16x changes the normalized
quality automation mapping, so check old DAW automation envelopes after upgrading.

Regression coverage includes primitive derivatives, custom curves with bias and
asymmetry, selected alias reduction, exact host bypass, disabled-section impulse
equivalence, clean 808 preservation, latency, state migration and callback
allocation checks. Measurements of individual folded harmonics are controlled
examples, not promises for every sound or setting.
