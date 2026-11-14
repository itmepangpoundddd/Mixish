# Mixish UI audit (current state)

Screenshot: `mixish_audit_main.png` (1366x738 window, 1366x768 screen).
No code was changed for this audit.

Scope: all files under `frontend/widgets/mixish/`.

## 1. Colour system

There is no palette. The Mixish code contains **80+ distinct hex colours**.

Same meaning, different colours:

- Preview green: `#1D9E75` (MixishSwitcher.cpp headers, MixishInputTile.cpp border,
  MixishVideoControls.cpp seek bar), `#3BD6A2` (MixishInputTile.cpp PVW badge),
  `#7FD6A7` (MixishAudioMixer.cpp section label).
- Program/on-air red: `#C0392B` (MixishSwitcher.cpp headers, MixishInputTile.cpp
  border, MixishBottomBar.cpp REC state), `#E5484D` (MixishOverlayPanel.cpp
  on-air, MixishBottomBar.cpp LIVE border), `#FF6B5E` (MixishInputTile.cpp PGM
  badge), `#D14537` (hover).
- Overlay ON is **green** `#0E7A0E` / `#12900F` / `#4CC94C` / `#0A5F0A`
  (MixishOverlayStrip.cpp) while Preview is a different green. Green currently
  means "preview", "overlay on", and "success".
- Danger and on-air share red: `#FF6B5E` / `#7D1224` (MixishInputTile.cpp,
  unused delete styles), `#C0392B` used for program and recording.
- "Selected/active" has three different cues: white fill + 2px border
  (MixishTransitionPanel.cpp), coloured 2px border (MixishInputTile.cpp), dark
  green fill (MixishOverlayStrip.cpp), and a legacy blue `#4C8DFF`.
- Channel identity colours are still used as UI colour in
  MixishOverlayPanel.cpp (`#4C8DFF`, `#39C77A`, `#F2A93B`, `#A97BFF`) and
  MixishOverlayController.cpp (`ChannelColor()`), while the strip no longer
  uses them.
- Neutrals: more than 40 shades between `#080808` and `#FFFFFF`; the "panel
  background" alone uses `#141414`, `#161616`, `#181818`, `#1A1A1A`, `#1B1B1B`,
  `#1E1E1E`, `#202020`, `#232323`, `#242424`, `#2A2A2A`, `#2E2E2E`.
- Text greys: `#F1F1F1`, `#E8E8E8`, `#E4E4E4`, `#DADADA`, `#D6D6D6`, `#C8C8C8`,
  `#B8B8B8`, `#A8A8A8`, `#A8A69E`, `#8C8C8C`, `#777777`, `#6E6E6E`, `#5E5E5E`.

## 2. Typography

- Nine font sizes in use: 8, 9, 10, 11, 12, 13, 14, 15, 20 px.
- Weights: 600 and 700 used interchangeably for the same role.
- Section labels are all uppercase but differ in size/colour: "INPUTS"
  (MixishInputBar.cpp, app font +1 pt, `#F1F1F1`), "TRANSITIONS"
  (MixishTransitionPanel.cpp, 14 px), "OVERLAYS" (MixishOverlayStrip.cpp, 10 px,
  `#A8A69E`), "AUDIO MIXER" (MixishAudioMixer.cpp, 11 px), "MASTER" (9 px),
  "OVERLAY CHANNELS" (MixishOverlayPanel.cpp, 13 px).
- Case is mixed in buttons: `CUT`, `FADE`, `FTB`, `All Off`, `Edit`, `Cut`,
  `Settings`, `Record`, `Stream`, `REC`, `LIVE`, while transitions keep their
  OBS names (`Stinger`, `Fade to Color`).

## 3. Buttons

- Heights in use: 20 (overlay Edit), 22 (tile Settings, overflow), 24 (All Off,
  Audio Mixer toggle), 26 (bottom bar, tile menu), 28 (transport), 30 (T-bar
  min), 34 (CUT/FADE/FTB/custom transitions), 36 (overlay toggles), 38/40 (bars).
- Corner radii: 2, 3, 4 px (and 0 for flat toolbuttons).
- Borders: 1 px `#333333`, `#3C3C3C`, `#5A5A5A`, `#555555`; active states use
  2 px `#FFFFFF` (MixishTransitionPanel.cpp) or 2 px coloured
  (MixishInputTile.cpp).
- Icon vs text is inconsistent: tile "Settings" = icon + text, "Cut" = text,
  overflow = glyph `⋮`; transition gears = icon-only; top bar = text;
  Add Input = text + arrow; video transport = OBS icon-only.
- Disabled text uses `#777777`, `#5E5E5E`, `#6E6E6E`, or `#9A9A9A` depending on
  the file.

## 4. Spacing and heights

- Bar heights are unrelated: top bar min 38 (MixishTopBar.cpp), preview/program
  headers 34 (MixishSwitcher.cpp), INPUTS header ~30 (implicit), bottom bar 38
  (MixishBottomBar.cpp).
- Panel padding: 8/5 (bottom bar), 8/4 (input bar), 8/6/8/8 (overlay strip),
  8/6 (transition panel), 4 (tile), 10 (overlay panel), 6/5 (mixer).
- Layout spacing: 1, 2, 4, 6, 8 px mixed in the same regions.
- INPUTS header is crowded: title + 8 colour swatches + transport (min width
  340) + Audio Mixer toggle in one row; at 1100 px the seek bar drops to ~110 px.
- Audio mixer is `min 280 / max 360` (MixishAudioMixer.cpp), not resizable, and
  its scroll area only shows about three 44 px rows at 1366x768; more sources
  require scrolling inside a small area.
- Bottom bar puts Add Input at the far left and everything else at the far
  right, leaving a large empty middle; the gears are 26x26 next to 126 px
  buttons but the 6 px spacing makes them read as separate controls.
- Transition column mixes full-width 34 px buttons, a 30 px T-bar, 22 px
  presets, a 20 px Edit, a 24 px All Off, and 42x36 overlay toggles.
- Tiles are 208x174 with a 190x107 thumbnail; footer buttons are 22 px high and
  the number label is 14 px wide.

## 5. Inline styling

There is no stylesheet file. `setStyleSheet()` is called 79 times in 12 files:

| File | calls |
| --- | --- |
| MixishAddInputController.cpp | 9 |
| MixishAudioMixer.cpp | 3 |
| MixishBottomBar.cpp | 8 |
| MixishInputBar.cpp | 6 |
| MixishInputProperties.cpp | 3 |
| MixishInputTile.cpp | 9 |
| MixishOverlayPanel.cpp | 9 |
| MixishOverlayStrip.cpp | 4 |
| MixishSwitcher.cpp | 5 |
| MixishTopBar.cpp | 2 |
| MixishTransitionPanel.cpp | 9 |
| MixishVideoControls.cpp | 2 |

The preview/program headers are styled from MixishSwitcher.cpp
(`kPreviewHeaderStyle`, `kProgramHeaderStyle`), so their 34 px height and
`#F1EFE8` text are set outside any theme.

Dead styles: the red tile delete button rules in MixishInputTile.cpp remain
after the button moved to the overflow menu.

## 6. Semantic conflicts to resolve

1. Green means Preview **and** overlay ON **and** success.
2. Red means Program **and** recording/streaming **and** danger.
3. "Selected" means white fill, coloured border, or dark green fill depending on
   the widget.
4. Section titles use three sizes and two colours.
5. Four disabled text colours.

## 7. OBS-owned widgets inside Mixish

`MediaControls` (transport), `VolumeControl` (mixer rows) and the T-bar keep
OBS's own theme colours. They are reused intentionally; only their containers
should be themed. The mixer meters must keep the green/yellow/red level ramp
because those colours encode audio level, not app state.
