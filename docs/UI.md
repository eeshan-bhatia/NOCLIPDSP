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

Version 0.4.0 uses CLIP, SHAPE and 808 ENGINE pages. All continuous controls are
rotary knobs. SHAPE and 808 have a GAIN / MIX view for shared controls. The former
multiband page and its host controls were removed to keep the product focused.
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

Graph updates stay at 30 Hz and glows use JUCE vector drawing. The trace previews
the settled full input-to-output chain using a separate AudioEngine. Dashed blue
guides show positive and negative sample ceiling amplitudes when clipping is active.
The CLIP graph is a static transfer preview excluding DC-filter history. SHAPE/808
show actual latest-block waveform snapshots (blue input, amber output). These are
approximate GUI samples, not synchronized recordings or true-peak meters.

Four fixed meter cells render digits in fixed character slots at no more than 8 Hz.
Peaks hold for 350 ms then fall at 12 dB/s. RMS power and clipping fraction use
300 ms exponential averaging. These display ballistics sample the processor's
latest block measurements; they are not integrated loudness or exhaustive peak capture.

Font-specific handling: this font contains decorative ASCII hyphen glyphs.
Numeric formatting uses Unicode minus with a standard fallback glyph, and accepts
both Unicode minus and ASCII hyphen during editing. All normal text remains in
Counter-Strike. Text sizes/horizontal scaling account for the unusually wide font.
Explicit slider text-box colours enforce dark text on a light tan field.
