# Sidechain and whole-plugin comparison — 0.5.0

## Sidechain

Route the kick (or another key) into the plugin's optional Sidechain input in your
host. Enable EXTERNAL SIDECHAIN on the SIDECHAIN page. The route is disabled by
default. Mono and stereo keys are supported; stereo uses the larger absolute
sample, so both audio channels receive the same control. The key is never audible.

- SENSITIVITY sets detector onset in dBFS. Full relief occurs 12 dB above it.
- DRIVE RELIEF sets maximum drive reduction, up to 24 dB.
- ATTACK and RELEASE set key-envelope response. An additional 5 ms smoother
  prevents instantaneous gain-control changes.

This is **drive relief**, not volume ducking or kick/sub spectral collision
removal. It eases global and 808 dirty-branch drive toward unity. It cannot reduce
unity drive further, undo threshold clipping, remove DC bias or guarantee a quiet
output. The clean sub path is untouched, although the usual output/mix/ceiling
still applies. With no key connection or amount zero it settles to neutral.
NO SIDECHAIN ROUTE means the host bus is disabled; a silent enabled bus reads
zero relief rather than implying that the host has routed a live source.

The sidebar status reports requested detector relief; the applied reduction may
be smaller when the unity-drive floor is reached. No extra lookahead is added.
Standalone has no DAW sidechain routing; test routing in a host such as FL Studio.

## Whole-plugin A/B

The A and B buttons above the controls select complete configurations. Adjust A,
use A TO B to seed B, select B and edit it. A TO B / B TO A replaces the destination,
including curves, 808 controls, sidechain and oversampling. Both configurations
and the selected slot are saved in the host project. DSP parameter automation
always edits the selected slot. These are independent of EDIT A / EDIT B and
curve morph inside SHAPE.

RMS MATCH runs both configurations on the same input, measures output energy,
and gradually attenuates the louder one by at most 12 dB. It never boosts either
result. Start playback and allow a few seconds to settle before comparing.
A 400 ms energy follower, 400 ms observation minimum, 120 ms bank warm-up and
500 ms trim smoothing avoid sudden correction. Very quiet/silent signals do not
establish a valid match. Changes in source or settings require settling again.

MATCH costs roughly a second processing chain; oversampling remains the main CPU
cost. With MATCH off only the audible chain runs, except during switching.
Snapshot switches warm the other chain then crossfade over 20 ms. All paths keep
the existing fixed 65-sample host latency. Host bypass returns exact delayed raw
audio without match attenuation. The plugin's saved BYPASS control remains part
of the comparison and may be attenuated when matching it against the other slot.

RMS is not LUFS and does not make differently distorted signals perceptually
equal. Differences exceeding 12 dB may remain. Ceilings are sample-based; no
true-peak guarantee is added by comparison.

## Validation

StateTests covers snapshot recall/copy, old-schema migration, invalid inactive
state rejection, mono/stereo key layouts, key isolation, drive relief, clean 808
branches, 6 dB RMS comparison, smooth switching and exact host bypass. Existing
allocation monitoring includes active matching, sidechain and snapshot switching.
Offscreen editor renders check the compact 808 and comparison layouts.
FL Studio routing, listening and project recall still need real host evaluation.
