# Whole-plugin A/B

A and B store all four shapers, their curves, section switches, crossovers, 808,
global gain/mix/ceiling and oversampling. Edit A, copy A TO B, then edit B and
compare. Copy replaces the destination. These are separate from CURVE 1/2 and
Morph inside each section.

RMS MATCH runs both configurations on identical input, attenuating the louder
one by at most 12 dB. It never boosts. Allow a few seconds to settle. A 400 ms
energy follower, 400 ms observation minimum, 120 ms warm-up and 500 ms trim
smoothing avoid abrupt corrections. Silence cannot establish a valid match.
RMS matching is not LUFS or equal perceived loudness; differences over 12 dB can
remain. MATCH is global and unchanged when switching A/B.

Normally only the audible chain runs; switches warm the other then crossfade for
20 ms. MATCH runs both continuously and increases CPU. All paths keep fixed
68-sample latency. Settled host bypass returns delayed raw audio without trim.
The saved plugin BYPASS can itself be compared and attenuated by MATCH.

External sidechain routing and controls were removed in 0.6.0.
