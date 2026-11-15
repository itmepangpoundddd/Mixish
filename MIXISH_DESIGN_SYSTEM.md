# Mixish design system

Status: **approved and applied** (Step 3). See "Applied deltas" at the end.
Applied stylesheet: `frontend/forms/mixish/mixish.qss`.

Companion audit: `MIXISH_UI_AUDIT.md`.
Reference screenshot: `mixish_audit_main.png`.

## 1. Semantic colour tokens

| Token | Value | Meaning / usage |
| --- | --- | --- |
| `--preview` | `#1D9E75` | Preview only: Preview header, Preview tile border, PVW badge |
| `--program` | `#C0392B` | Program / on-air only: Program header, Program tile border, PGM badge, overlay ON, REC/LIVE |
| `--program-hover` | `#D14537` | Hover/pressed shade of on-air controls |
| `--accent` | `#4C8DFF` | Generic selected/active/focus/progress: selected transition, selected preset, seek bar progress, focus ring |
| `--on-accent` | `#0B1220` | Text/icon on accent |
| `--on-color` | `#F7F7F2` | Text/icon on preview/program/danger fills |
| `--danger` | `#E5484D` | Destructive actions only (Delete input, Remove layer) |
| `--danger-hover` | `#F25C5F` | Danger hover |
| `--danger-surface` | `rgba(229, 72, 77, 0.12)` | Danger button background |

Neutrals:

| Token | Value | Usage |
| --- | --- | --- |
| `--bg-app` | `#101010` | Window / dock background |
| `--bg-panel` | `#181818` | Top bar, INPUTS bar, transition column, bottom bar, mixer |
| `--bg-surface` | `#202020` | Tiles, panel cards, list rows |
| `--bg-elevated` | `#2A2A2A` | Buttons, combo boxes, spin boxes, inputs |
| `--bg-hover` | `#333333` | Hover |
| `--bg-pressed` | `#1C1C1C` | Pressed |
| `--border` | `#3A3A3A` | Default 1 px border |
| `--border-strong` | `#555555` | Hover border, scrollbar handle |
| `--text-primary` | `#F1F1F1` | Body text, button labels |
| `--text-secondary` | `#B8B8B8` | Secondary labels |
| `--text-muted` | `#8C8C8C` | Section labels, hints |
| `--text-disabled` | `#5E5E5E` | Disabled text |

Rules:

1. Green is **only** Preview. Nothing else uses green (except OBS audio meters,
   see section 8).
2. Red is **only** Program/on-air, including overlay ON, REC and LIVE.
   Destructive actions use `--danger`, which is a different red.
3. Blue accent is **only** generic selection/active/progress/focus.
4. No other colours are allowed in our widgets. Channel identity (OV chip
   colours in the overlay editor) becomes neutral: channel is identified by its
   number and title, not by a colour.

## 2. Typography

| Token | Value |
| --- | --- |
| `--font-family` | inherit OBS application font |
| `--size-label` | `10px` |
| `--size-body` | `12px` |
| `--size-title` | `14px` |
| `--weight-regular` | `400` |
| `--weight-bold` | `700` |
| `--tracking-label` | `1px` (uppercase section labels only) |

Case rule:

- **Section labels**: uppercase, bold 10 px, `--text-muted`, 1 px tracking.
  Applies to INPUTS, TRANSITIONS, OVERLAYS, AUDIO MIXER, MASTER, T-BAR,
  field-group labels in dialogs.
- **Everything else**: sentence case (buttons, tile names, dialog titles,
  transition names). OBS transition names are shown as-is but not
  upper-cased.
- Buttons: bold 12 px. Body text and time values: regular/medium 12 px.
  Titles: bold 14 px.
- The 8 px and 9 px text and 15/20 px display sizes are removed.

## 3. Spacing, radius, borders

| Token | Value |
| --- | --- |
| `--space-1` | `4px` |
| `--space-2` | `8px` |
| `--space-3` | `12px` |
| `--space-4` | `16px` |
| `--radius` | `4px` (all controls and panels) |
| `--border-width` | `1px` |
| `--focus-width` | `1px` accent (replaces the current 2 px white) |

Grid rules:

- Panel padding: 12 px. Row gap: 8 px. Inline gap: 4 px. Group separation: 16 px.
- All bar content aligns to the 4 px grid.
- No 1/2/6 px spacing.

## 4. Button variants

Only four variants, selected with the dynamic property `mixishVariant`:

| Variant | Look | Usage |
| --- | --- | --- |
| `primary` | filled `--accent`, `--on-accent` text, no border | the single confirming action (Create Input, OK, Apply) |
| `secondary` | `--bg-elevated`, `--text-primary`, 1 px `--border`; hover `--bg-hover` + `--border-strong`; pressed `--bg-pressed` | default for every normal action |
| `icon` | secondary look, fixed 28×28, 16 px icon, no text, tooltip required | gear, close, overflow, transport |
| `danger` | `--danger-surface` bg, `--danger` text, 1 px `--danger` border; hover filled `--danger` + `--on-color` | Delete input and other destructive actions |

States:

- `toggled` / selected (generic): filled `--accent`, `--on-accent` text.
- `onAir`: filled `--program`, `--on-color` text. Overrides `toggled`.
- `preview`: 1 px `--preview` border + `--preview` text on a neutral surface
  (used for PVW badges; headers are filled).
- `disabled`: `--bg-surface`, `--text-disabled`, `--border` at 50% opacity,
  no hover.
- `ghost` (empty overlay channel): `--bg-surface`, `--text-muted`,
  `--border`; still clickable.
- Buttons never use 2 px borders or white fills.

## 5. Fixed region heights

| Region | Height |
| --- | --- |
| Top bar | `36px` |
| Preview/Program header | `32px` |
| INPUTS header row | `40px` |
| Bottom bar | `40px` |

Control heights:

| Control | Height |
| --- | --- |
| Primary/secondary button (bars, dialogs) | `32px` |
| Standard button (transition buttons, panel rows) | `32px` |
| Compact button (presets, overlay Edit, All Off, tile footer) | `24px` |
| Icon button | `28 × 28px` |
| Inputs (combo/spin/text) | `28px` |
| Overlay toggle | `40 × 32px` |
| Audio mixer row | `44px` |

## 6. Component specs

### Top bar
36 px, `--bg-panel`, 1 px bottom border. Logo "MIXISH" 14 px bold uppercase,
1 px tracking. Menu buttons are secondary text buttons 28 px high, 12 px bold.
Separators 1 px `--border`.

### Preview / Program headers
32 px, filled `--preview` / `--program`. Label uppercase 12 px bold + scene name
sentence case 12 px bold, both `--on-color`. No other colours.

### Transition column
Width 232 px, padding 8 px, 4 px gaps, `--bg-panel`.
- Section label row: "TRANSITIONS" (section label) + two 28 px icon buttons.
- CUT, FADE, FTB and every custom transition: 32 px secondary, full width.
  The current transition uses the generic `toggled` accent state.
- Duration presets: row of four 24 px compact buttons; active preset = accent.
- Duration spin box: 28 px.
- "T-BAR" section label + 28 px T-bar.
- "OVERLAYS" section label row with `Edit` as a 24 px **secondary** button.
- Four overlay toggles: 40×32. Empty = ghost, assigned = secondary, ON = `--program`.
- `All Off`: 24 px secondary, full width.
- The whole content scrolls; All Off stays pinned at the bottom.

### INPUTS header row
40 px, `--bg-panel`, padding 0 12 px, 12 px gaps.
Contents only: `INPUTS` label, media transport (play/pause, stop, previous,
next, restart; seek bar; current/total time), `Audio Mixer` secondary toggle
(28 px). The 8 colour swatches are removed; colour tags stay in each tile's
overflow menu.

### Media transport
Icon buttons 24×24 (OBS MediaControls), seek bar flex with minimum 160 px,
groove 4 px `--bg-elevated`, progress `--accent`, handle 10 px `--text-primary`,
time labels 12 px regular. Disabled group uses `--text-disabled`.

### Input tiles
208 × 196 px, `--bg-surface`, 1 px `--border`, radius 4.
- Header 26 px `--bg-elevated`: number 12 px bold, name 12 px bold ellipsized,
  badges 10 px bold uppercase (PVW green outline, PGM red outline).
- Thumbnail 190×102 with the checkerboard backdrop.
- Footer, two rows of 24 px secondary buttons:
  - row 1: `Cut` + overlay channels `1 2 3 4`;
  - row 2: `Audio` (or `Muted`) + `Settings` + overflow `⋮`.
- Clicking an overlay number assigns the tile to that channel and turns it on;
  clicking it again turns the channel off. `Audio` mutes the input's primary
  audio source (shared state with the mixer).
- State: Preview = 1 px `--preview` border; Program = 1 px `--program` border;
  selected but not on air = 1 px `--accent` border. Program wins over Preview,
  Preview wins over selected.

### Audio mixer
Resizable via a vertical splitter between the tile row and the mixer.
Minimum width 280 px, default 300 px, maximum 480 px; minimum height follows
the INPUTS row. `--bg-panel`, 1 px left border, 12 px padding.
- Header: "AUDIO MIXER" section label.
- A horizontal console of vertical channel strips (vMix-style): each source is
  an 80 px card containing OBS's `VolumeControl` in vertical mode — name menu,
  dB readout, vertical fader, stereo meter, mute and monitor buttons.
- The MASTER strip is pinned on the right and always visible: an 80 px
  accent-bordered card with a `MASTER` label, dB readout, vertical −60..+6 dB
  fader (0.1 dB steps), a 10 px post-gain peak meter (green → yellow → red,
  50 ms polling) and a full-width `M` button (red when muted). The fader and
  meter read/write the libobs master bus, so they affect the encoded program
  mix and not just monitoring.
- All audio inputs are listed, including Desktop Audio. New sources appear
  without restarting.
- Strips keep a 140 px minimum height and scroll horizontally when they exceed
  the panel width; they are never compressed or clipped.
- Empty state: 12 px muted text.

### Bottom bar
40 px, `--bg-panel`, 1 px top border, padding 0 12 px.
- Left group: `Add Input` secondary split button with dropdown arrow, 32 px,
  including `Add Scene (legacy)` in the menu.
- Right group 1: gear icon 28×28 immediately followed by `Record` (128 px).
- Right group 2: gear icon 28×28 immediately followed by `Stream` split button
  (128 px). Group gap 16 px.
- Idle = secondary. Active = `--program` filled with label + live time, e.g.
  `REC 00:12:34` / `LIVE 00:12:34`, 12 px bold.
- No centre hint text.

### Dialogs and panels
`--bg-panel`, 12 px padding, 8 px row gaps. Title 14 px bold sentence case.
Field labels 10 px section style. Inputs and buttons 28 px. Confirming action
`primary`, cancel `secondary`, delete `danger`. Overlay editor channel chips
become neutral numbered labels (no per-channel colour).

### Scrollbars
8 px wide, handle `--border-strong`, radius 4, transparent track, no arrows.

## 7. State mapping (old → new)

| Meaning | Old | New |
| --- | --- | --- |
| Preview header/tile/badge | `#1D9E75`, `#3BD6A2` | `--preview` |
| Program header/tile/badge | `#C0392B`, `#FF6B5E` | `--program` |
| Overlay ON | `#0E7A0E` | `--program` |
| Overlay assigned/off | grey `#454545` | `secondary` |
| Overlay empty | grey `#3C3C3C` | `ghost` |
| Active transition / preset | white `#F1F1F1` | `--accent` |
| Selected tile (not on air) | coloured 2 px border | `--accent` 1 px border |
| Seek progress | `#1D9E75` | `--accent` |
| REC/LIVE | `#C0392B` on `#242424` | `--program` filled |
| Delete/danger | `#FF6B5E`, `#7D1224` | `--danger` variant |
| Generic button | `#2E2E2E` border `#3C3C3C` | `secondary` |
| Disabled | four greys | `--text-disabled` |

## 8. Allowed exceptions

- OBS audio meters keep the green/yellow/red level ramp: those colours encode
  signal level, not application state.
- OBS's `MediaControls` buttons keep their icon set; only size, spacing, hover
  and disabled colours are themed.
- The mixer/T-bar sliders keep OBS's handle shape; colors follow the tokens.

## 9. Implementation plan (after approval)

1. Add `frontend/widgets/mixish/MixishTheme.hpp/.cpp` and
   `frontend/widgets/mixish/mixish.qss` (loaded from a Qt resource).
2. Apply the QSS to the Mixish root widgets only, with selectors scoped by
   object name (`#mixishTopBar`, `#mixishInputBar`, ...), so OBS's classic
   docks and dialogs are untouched.
3. Replace every inline `setStyleSheet()` in Mixish files with a dynamic
   property (`mixishVariant`, `mixishState`) and one `MixishTheme::Polish()`
   call per state change.
4. Move the hardcoded heights/margins to the token values listed above.
5. Remove the INPUTS colour swatch row (colour tag already exists in the tile
   overflow menu).
6. Make the tiles/mixer a splitter; keep all signals and hotkeys unchanged.
7. Verify at 1366×768 and 1100×640 with the mixer hidden and shown; grep to
   confirm `setStyleSheet` only exists in `MixishTheme.cpp`.
8. Build `frontend\obs-studio` only, then run 10 shutdown cycles again.

## 10. Decisions needed

1. Overlay ON becomes **red** (`--program`) instead of the current green — ok?
2. Active transition/preset becomes **accent blue** instead of white — ok?
3. Three text sizes means badge text grows 8 → 10 px and titles shrink 15 → 14
   px — ok?
4. Region heights: top 36, INPUTS 40, bottom 40, monitor headers 32 — ok?
5. Tiles: `Cut` / `Settings` / `⋯` all become text-only 24 px buttons (the
   Settings gear icon is dropped on the tile) — ok?
6. Mixer becomes a user-resizable splitter (min 280, default 300, max 480) — ok?
7. Accent blue `#4C8DFF` is the generic selection colour — ok, or prefer neutral
   white?
8. Keep OBS meter colours as the only green/red exception — ok?

## Applied deltas (Step 3)

- Transition column is 232 px wide (not 180) so the duration spin box and the two header icon buttons fit without clipping.
- The duration spin box sits in the TRANSITIONS header row; it shows the millisecond value without a suffix and explains the unit in its tooltip. The 250/500/1000/2000 presets keep their own row.
- All Off moved into the OVERLAYS header row next to Edit. It stays visible at every window size and the transition column no longer needs an inner scrollbar at 1366x768.
- Tiles are 208x196 px with a two-row footer:
  - row 1: `Cut` + overlay assignment buttons `1 2 3 4`;
  - row 2: `Audio` (shows `Muted` when the input is muted) + `Settings` + overflow `⋮`.
  Clicking an overlay number assigns the tile's scene to that channel and turns it on; clicking the same number again toggles it off. The active channel button is red (on-air), an assigned channel is neutral.
- Audio mixer rows keep a 40 px minimum height and the panel is resizable through the INPUTS splitter (minimum 280 px, no maximum).
- INPUTS bar minimum height is 248 px (40 px header + 196 px tile + margins/handle).
- The OBS properties form is dark like the rest of Mixish (labels, inputs, groups and scroll areas use the same tokens), so nothing is white-on-light.
- setStyleSheet() remains only in MixishTheme.cpp (the loader) and as style clearing when leaving the Mixish layout.
- Dropdown buttons use an 8 px chevron with an 18 px menu-button area; the bottom-bar gear buttons are 32x32 to match Record/Stream.
- Tile thumbnails request all visible tiles every 250 ms, so previews update several times per second.
- Mixer channels are vertical cards; the MASTER strip is pinned on the right and uses the accent border. OBS's "Active/Global" category label is hidden.
- The master strip is a real bus, not an output-device fader: post-mix gain, mute and peak are provided by `obs_set_master_volume_db()` / `obs_get_master_peak()` in libobs and persisted as `Mixish/MasterVolumeDb` / `MasterMuted`.

## Settings dialogs and properties page (vMix-aligned layout)

- Recording Setup: profile buttons 1-2, format list (MP4/MKV/MOV/FLV/MPEG-TS), folder + Browse, canvas size and FPS summary, bitrate/quality, encoder + hardware toggle, audio track/bitrate, disabled split/WAV rows, OK/Cancel. It edits OBS Simple Output keys and calls ResetOutputs.
- Streaming Settings: profile combo with Add/Delete plus numbered profiles (up to 5, stored in the user config), Destination, URL, Stream Name or Key, quality presets, FFMPEG application, hardware toggle, Stop All / Stop 1 / View Status / Save and Close. Saving updates the OBS service (server/key) and Simple Output encoder keys, then resets outputs and saves the collection.
- The Properties dialog now has a full-height sidebar and a large live preview below the settings content (right of the sidebar), matching the vMix layout. The sidebar categories remain the OBS property groups.
- Both settings dialogs are modal, use the shared theme, and refuse to apply while the matching output is active.
