# Architecture (0.4.0)

AudioEngine owns gain/mix/enable ramps, meters and AdvancedDSP. Clipper.h contains
pure built-in transfers; CurveTable.h implements bounded curves and the publication
mailbox; Crossover.h implements TPT Linkwitz-Riley filters. The three-band prototype
remains tested but is not exposed in the plugin. Stereo filter states are separate;
control ramps and the Punch detector are linked across channels.

ProcessingChain owns four JUCE FIR oversamplers, independent engines, fixed delay
rings and work buffers. Prepare allocates; processBlock never resizes. Arbitrary
host buffers are chunked into at most 256 frames. All banks are padded to the maximum
integer delay (65 samples in the pinned build), also applied to raw bypass. Quality
changes warm a preallocated replacement for 100 ms then crossfade for 20 ms. Only
the selected bank and a transitioning bank run. No callback latency changes,
filter construction, locks or I/O are used.

Wet/dry mixing occurs inside the common oversampled domain. 808 dry follows the
same crossover phase response as its wet branch. A final host-rate sample guard
catches reconstruction overshoots; it is not true-peak limiting.

APVTS stores nine ordinates for each A/B curve at fixed ordered abscissae. A worker
polls these at roughly 10 ms, builds two 2049-entry LUTs and publishes through a
three-slot single-producer/single-consumer mailbox. Atomic slot exchange ensures
exclusive ownership. Audio copies completed data into preallocated engines and
crossfades over 20 ms, rejecting stale tables. Prepare builds the current tables
synchronously so recalled curves are present before the first audio block. Curve
points are saved editor controls, not sample-accurate host automation; continuous
A/B morph is smoothed and automatable.

Schema 5 validates IDs, bounds, finite values and discrete choices. Schemas 1/2
initialize new processing conservatively; development schema 3 migrates 808 routing
and removes abandoned band controls. Original host class IDs/state root remain.
Raw parameter atomics are cached once. Meter and latest-block scope data use
lock-free atomics; scope is an approximate preview, not a synchronized recording.

The UI provides CLIP, SHAPE and 808 pages and a GAIN / MIX subview. No GUI component
writes audio buffers. The existing logo is embedded unchanged; high-quality raster
scaling and a vector overdraw preserve its thin upper crosshair at small sizes.

ComparisonEngine owns two heap-allocated/preallocated ProcessingChain instances.
Only the audible chain runs normally; both run during a 120 ms warm-up / 20 ms
snapshot crossfade or while MATCH is enabled. Both see identical main/key input.
The 400 ms energy follower targets attenuation of the louder output (maximum
12 dB), with a 500 ms gain smoother. Measurement starts after warm-up, requires
400 ms of observation and both mean-square levels above 1e-8. Silence does not
produce division by zero or gain boosts. Host bypass skips matching gain.

GUI/state snapshot transactions use a non-audio mutex and an atomic revision.
The audio callback makes one bounded read attempt and keeps its prior settings
if a transaction overlaps; it never waits. The worker publishes curves for both
slots; pending LUTs are retained until matching audio settings become current.
Prepare synchronously installs both curves. APVTS host automation addresses the
selected snapshot; selecting/copying snapshots notifies the host of their values.

Each chain has a host-rate linked absolute-peak sidechain envelope, attack/release
poles and a 5 ms relief smoother. Sensitivity sets onset, with full configured
relief at 12 dB above onset. Relief multiplies nonlinear drive with a unity floor;
clean 808 branches are untouched. Control is held over each oversampled frame;
there is no added lookahead or latency. The detector is separate from main audio.

See PHASE34.md and SIDECHAIN_AB.md for usage, equations and limits.
