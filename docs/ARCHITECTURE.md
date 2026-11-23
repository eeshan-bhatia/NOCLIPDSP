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

Schema 4 validates IDs, bounds, finite values and discrete choices. Schemas 1/2
initialize new processing conservatively; development schema 3 migrates 808 routing
and removes abandoned band controls. Original host class IDs/state root remain.
Raw parameter atomics are cached once. Meter and latest-block scope data use
lock-free atomics; scope is an approximate preview, not a synchronized recording.

The UI provides CLIP, SHAPE and 808 pages and a GAIN / MIX subview. No GUI component
writes audio buffers. The existing logo is embedded unchanged; high-quality raster
scaling and a vector overdraw preserve its thin upper crosshair at small sizes.

See PHASE34.md for equations, measurements and limits. Sidechain-aware control and
level-matched whole-plugin A/B follow the simplified SHAPE/808 release.
