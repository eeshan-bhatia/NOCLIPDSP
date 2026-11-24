# UI design

User-supplied visual direction: early-2000s PC game menus and executable windows,
Counter-Strike Regular typography, beveled sand panels, inset olive dials,
a restrained technical readout, and green/orange glow accents.

| Role | Colour |
| --- | --- |
| Main background | #ccba7c |
| Text | #0c0f12 |
| Graph and dial background | #413a27 |
| Graph trace | #de9b35 |
| Filled knob arc | #013220 |
| Sparse version/selection accent | #5d79ae |
| Bevel highlight / numeric field | #e5d69f |
| Borders / secondary ticks | #82764e |
| Glow highlight | #a2ff57 |

The beige background uses a cached, fixed-seed sand-grain tile. It is generated
once per editor on the message thread and stays still during repainting/resizing.
Dark green knob arcs have four bright translucent glow layers around a solid fill.
The title is dark text with a green shadow glow. The graph uses layered
translucent vector strokes around its orange trace. Knob arcs have restrained
green highlights; readouts remain high contrast. No game artwork/logos are used.

Version 0.6.0 has five view-only tabs: MASTER, LOW, MID, HIGH and 808.
Each has a separate ON switch. Shared gains, ceiling/mix and crossovers remain
visible. Four shaper views combine analytical CLIP left and editable SHAPE right.
808 retains a compact six-knob view. All continuous controls use rotary knobs.

Theme is strictly editor-view based: the 808 tab uses #788e9d background, white
text, dark slate surfaces and blue glow; leaving it restores every control to the
sand/green palette even when 808 remains enabled. Tests exercise 808 -> each of
the other four tabs and compare complete saved audio state before and after.

The existing user-supplied scope logo remains unchanged; high-quality scaling
and a small vector overdraw preserve its thin upper crosshair at icon size.

TacticalLookAndFeel owns the shared embedded typeface and control drawing.
Font loading and GUI drawing occur on the message thread, never in audio
processing. The external font is embedded at build time only when
NOCLIP_FONT_FILE is supplied. The repo does not redistribute the user's TTF.
Its metadata credits "by SoJa"; no license text accompanied the file.

All controls remain APVTS-attached. The UI title is NOCLIP.DSP; application and
host product name are noclip.DSP. Assets/icon.png is the supplied original icon,
embedded in the editor and Windows application resources. Internal plugin IDs stay stable.
The knobs use a solid green arc without an internal stripe or pointer.

Graph updates stay at 30 Hz. The left preview shows the selected analytical clip
curve, while the right editor shows both custom endpoints and draggable points.
These are transfer views, not measured waveform or true-peak displays. Global
ceiling remains visible in its dedicated knob. UI scope atomics are retained
internally but no longer occupy a separate large waveform panel.

Four fixed meter cells render digits in fixed character slots at no more than 8 Hz.
Peaks hold for 350 ms then fall at 12 dB/s. RMS power and clipping fraction use
300 ms exponential averaging. These display ballistics sample the processor's
latest block measurements; they are not integrated loudness or exhaustive peak capture.

Font-specific handling: this font contains decorative ASCII hyphen glyphs.
Numeric formatting uses Unicode minus with a standard fallback glyph, and accepts
both Unicode minus and ASCII hyphen during editing. All normal text remains in
Counter-Strike. Text sizes/horizontal scaling account for the unusually wide font.
Explicit slider text-box colours enforce dark text on a light tan field.


Global Mix is the leftmost top-row knob, followed by Input, Output, Ceiling and
the two crossover controls. Reordering does not change parameter IDs or values.

The 808 page includes a blue phosphor-style signal monitor beside its six knobs.
Dim input and glowing output traces use 2048 samples of rolling low-pass-filtered
plugin audio history (left channel), with a 1024-sample displayed window. A rising zero crossing
steadies bass waveforms. Input is offset by the reported fixed latency for visual
comparison. This is approximate low-end visualization, not a spectrum, phase meter
or sample-accurate measurement. Atomic samples
avoid locks and allocation on the audio thread; drawing stays on the GUI thread.


808 monitor update: input/output traces now use separate analysis-only 24 dB/oct
LR low-pass filters following the 808 Split setting (40-400 Hz), with a smoothed
cutoff. The title is 808 SIGNAL MONITOR. The right-hand segmented meter measures
the larger filtered output peak across stereo channels, with 350 ms exponential
release. Its display spans -60 to 0 dBFS; GODMODE lights at -3 dBFS and above.
Waveforms remain left-channel previews; the footer shows the cutoff. GODMODE is a
visual level tier, not a processing mode. Filters never alter audible audio.

Meter styling: lit low-end segments are white. GODMODE is dim white below its
top tier and full white with layered white glow at -3 dBFS and above. The waveform
and other 808 accents remain blue. Offscreen on/off fixtures check the contrast.


Shaper visual feedback: amber points and endpoint lines remain editable. A white
RESULT overlay previews the settled local CLIP -> SHAPE -> Stage Mix transfer,
including Drive, Threshold, Hardness, Knee, Bias, Asymmetry and Morph. It previews
the section's configured sound even when its ON switch is off. Global gains,
global mix/ceiling, crossover phase and DC filtering are intentionally excluded.
The clip graph adds dashed horizontal threshold references and vertical input
references at threshold/drive; soft curves do not necessarily begin bending there.
The subtitle reads: 3 BAND WAVESHAPER WITH 808 FX.
