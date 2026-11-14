# Mixish Overlay Channels — Design Note

Status: proposal, awaiting approval.
Scope: 4 overlay channels (OV1–OV4) on top of Program, independent of
Cut/Fade/T-bar/Preview-Program. No libobs changes.

---

## 1. Verified findings (why the preferred approach works)

OBS renders the main canvas by walking **all 64 output channels in ascending
order into one texture**:

- `libobs/obs-view.c:118` `obs_view_render()` — loops `i = 0..MAX_CHANNELS`,
  `obs_source_video_render(view->channels[i])`, in order, same target.
- `libobs/obs-video.c:172` `render_main_texture()` — renders that view into
  `render_texture` at base resolution, then `render_output_texture()` scales it.
- `libobs/obs-video.c` encoders consume `mix->output_texture`, i.e. the same
  composite → **stream/record get the overlays**.
- `frontend/widgets/OBSBasic_StudioMode.cpp:319` Program monitor calls
  `obs_render_main_texture_src_color_only()` → **Program monitor gets the
  overlays**.
- `frontend/widgets/OBSBasic_Preview.cpp:163-170` Preview renders the preview
  scene **directly**, not the composite → **Preview stays clean** (desired).
- `MAX_CHANNELS` = 64 (`libobs/obs-defs.h:21`), public API
  `obs_set_output_source()` / `obs_get_output_source()` (`libobs/obs.c:1841`).

Channels already used by OBS itself:

| Channel | Owner |
|---|---|
| 0 | Program / transition (`OBSBasic_Transitions.cpp:280,435,438`) |
| 1–2 | Desktop Audio 1–2 (`OBSBasic_SceneCollections.cpp:826-827`) |
| 3–6 | Aux Audio 1–4 (`OBSBasic_SceneCollections.cpp:828-831`) |

Nothing else in the tree claims a fixed channel (only the test harness uses
0–5). **Mixish will use channels 16–19 for OV1–OV4**, so ordering is stable and
far from OBS-owned indices. OV4 renders last → topmost.

Audio also works through channels: `libobs/obs-audio.c:585-603` walks every
canvas channel and pushes its active tree into the mix. Duplicate individual
audio sources are detected (`push_audio_tree2`, `obs-audio.c:59-83`), tagged
`audio_is_duplicated`, skipped inside their parent scenes
(`obs-scene.c:1699-1712`) and mixed once as a root node → **no double audio**
when the same input is in Program and in an overlay.

Conclusion: the preferred approach works in Studio Mode, in the Program
monitor, in streaming and in recording. The multi-canvas feature does not
change this (it adds canvases, the main canvas/output path is unchanged).

---

## 2. Model

Each overlay channel `n` (0..3) owns a **private stage scene**
(`obs_scene_create_private("Mixish OV<n>")`) placed on channel `n + 16`.
The stage scene holds **one scene item**: the assigned Input (a Mixish Input is
an OBS scene). Keeping a stage scene per channel means:

- position / size / crop live on that channel's own scene item, so the same
  Input can be on two channels with different geometry;
- fade in/out uses OBS's own per-item show/hide transition path
  (`obs_scene.c:4054-4095`), the same mechanism as OBS's "Show/Hide
  Transition";
- the stage is private → not in the OBS scene list, not shown in the mixer,
  never written into the user's scene collection;
- OFF is "item hidden", so an overlay assigned but off costs nothing to render
  and produces no audio (`obs-scene.c:1079,1627-1634`).

Per channel state:

```
struct MixishOverlayChannel {
  OBSSource     stageScene;     // private scene, on channel 16+n
  OBSSceneItem  item;           // the assigned input scene
  std::string   inputUuid;      // persisted identity
  std::string   inputName;      // fallback resolve + tooltips
  bool          onAir = false;  // never persisted
  bool          audio = true;
  bool          fadeMode = true;    // Fade / Cut
  int           fadeInMs = 300;
  int           fadeOutMs = 300;
  QRectF        rect;               // x, y, w, h in canvas pixels
  obs_sceneitem_crop crop;
};
```

ON/OFF transitions do **not** touch channel 0, the transition source, the T-bar
or FTB: overlays are simply higher channels.

Fade: one private `fade_transition` source per channel item
(`obs_source_create_private`), assigned with
`obs_sceneitem_set_transition(show/hide)` + `..._set_transition_duration`.
Cut: toggle visibility, then `obs_transition_force_stop()` so the change is
instant (`obs_sceneitem_do_transition` forces 300 ms when duration is 0, so
force-stop is the clean way to cut).

Audio toggle: mute the **stage scene** (`obs_source_set_muted`), never the
input. If the input is also in Program, it is mixed once as a root node and
Program audio is unaffected; if it is overlay-only, muting the stage silences
it. The input's own mixer strip keeps working as today.

---

## 3. Position / size / crop

The stage item uses `OBS_BOUNDS_SCALE_STRETCH` + `OBS_BOUNDS_NONE`… i.e.
alignment top-left, `set_pos(x, y)`, `set_bounds(w, h)`, plus
`obs_sceneitem_set_crop`. Defaults on assign: full canvas
(`0,0,base_width,base_height`), so behaviour is "overlay covers everything"
until the operator resizes it.

Because there is **no Layers tab yet**, this work adds a reusable
`MixishCanvasEditor` widget: aspect-correct canvas view, draggable body, 8
resize handles, crop mode, numeric X/Y/W/H fields, reset-to-full-frame. It
edits an `obs_sceneitem` directly and is written so a future Layers tab can
reuse it unchanged.

---

## 4. Persistence

Private scenes are not saved by OBS (`libobs/obs.c:2571-2572`), so Mixish owns
the state. It is stored inside the scene collection's `modules` block
(`OBSBasic::Save`, `OBSBasic_SceneCollections.cpp:948-953`), key `mixish_overlay`:

```json
{ "version": 1,
  "channels": [
    { "input_uuid": "...", "input_name": "...", "audio": true,
      "transition": "fade", "fade_in_ms": 300, "fade_out_ms": 300,
      "x": 0, "y": 0, "w": 1920, "h": 1080,
      "crop": { "left": 0, "top": 0, "right": 0, "bottom": 0 } } ] }
```

ON/OFF is intentionally **not** stored: every start/collection load begins with
all overlays OFF. Restore runs after scenes are loaded
(`OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED` / finished loading), resolves
inputs by UUID then by name, and silently leaves a channel empty if the input
is gone.

This needs one new line in `OBSBasic.hpp` (`friend class
MixishOverlayController;`) to reach `collectionModuleData` — no logic change.

---

## 5. Safety

- **Cycles**: before assigning, walk the candidate input's scene tree
  (items, groups, nested scenes) and both directions of the relation; if the
  candidate contains the target stage scene, refuse with a message. Final
  backstop: `obs_scene_add()` returns NULL and logs "infinite source
  recursion" (`obs-scene.c:2281`).
- **Deleted input**: subscribe to the input scene source's `remove` signal.
  Any channel holding it turns OFF, the item is removed, the channel is
  cleared, and the strip/tile/panel refresh. Mixish's own Del path additionally
  warns first ("Input is on OV2 — delete anyway?").
- **Disappearing source** (device unplugged, file gone, media ended): the stage
  item keeps a ref to the scene; an input whose inner source dies keeps
  rendering its own background/nothing. No pointer is ever held without a ref;
  channel objects use UUIDs plus `OBSWeakSource` for UI.
- **Scene collection switch / shutdown**: OBS clears channels 0–63 in
  `ClearSceneData` (`OBSBasic_SceneCollections.cpp:1579-1581`), so Mixish
  releases all channels, items and transition sources, then re-applies on the
  next load. Destructor ordering is handled by a controller that is destroyed
  before the OBS main window finishes tearing down.
- **Other outputs**: projectors, screenshot and virtual camera use the main
  texture (VCam "Program" mode included) — they show overlays; VCam
  Scene/Source modes do not. Replay buffer shows overlays. This matches
  "overlays are part of Program".

---

## 6. UI

**Input tile bottom row** (`MixishInputTile`):

```
[ Cut ] [OV1][OV2][OV3][OV4] [ Audio ] [ Settings ]
```

- OVn: click assigns this Input to channel n and turns it ON; click again turns
  that channel OFF. Tooltip `Overlay n: <name>` / `Overlay n: empty`.
- states: idle (dim outline), assigned-off (channel colour fill at 25 %),
  on-air (full channel colour, dark text).
- Audio button moves onto the tile face (it exists only in the tile menu
  today); Del leaves the face — it lives in Settings/context menu.
- Mixed state per channel: mini table — assigned marker + ON-AIR marker.

Channel colours (Mixish palette, not copied from any product):
OV1 `#4C8DFF`, OV2 `#39C77A`, OV3 `#F2A93B`, OV4 `#A97BFF`.

**Transition column strip** (`MixishOverlayStrip`, below FTB): `1 2 3 4` + `All Off`.
Same colours/states as the tile row; a channel with no input is disabled but
still clickable to focus the Overlay panel.

**Bottom bar** (`MixishBottomBar`, new, Mixish layout only): left = `Overlay`
button that opens the panel + `Audio` mixer toggle. Step 5 will grow this bar;
it is created now in a way that leaves room.

**Overlay panel** (`MixishOverlayPanel`, non-modal tool window):
- 4 rows: colour swatch, assigned input (combo of Inputs + Clear), ON/OFF,
  audio, Cut/Fade combo, fade-in/out spin boxes.
- selected row → `MixishCanvasEditor` on the right + X/Y/W/H/crop fields.
- `All Off`, `Reset position`, live "ON AIR" label.
- everything reads/writes the controller, so the tile row, strip and panel stay
  in sync through one signal.

---

## 7. Files

New:

- `frontend/widgets/mixish/MixishOverlayController.hpp/.cpp` — model, stage
  scenes, channels, toggle, fade, audio, transform, save/load, hotkeys,
  remove-signal handling, cycle check.
- `frontend/widgets/mixish/MixishOverlayStrip.hpp/.cpp`
- `frontend/widgets/mixish/MixishOverlayPanel.hpp/.cpp`
- `frontend/widgets/mixish/MixishCanvasEditor.hpp/.cpp`
- `frontend/widgets/mixish/MixishBottomBar.hpp/.cpp`

Changed (minimal):

- `frontend/widgets/mixish/MixishSwitcher.hpp/.cpp` — owns the controller, adds
  strip + bottom bar, forwards frontend events, exposes helpers to tiles/panel.
- `frontend/widgets/mixish/MixishInputTile.hpp/.cpp` — new bottom row, OV
  buttons, tile audio button, Del moved to the menu.
- `frontend/widgets/mixish/MixishTransitionPanel.cpp` — overlay strip under FTB.
- `frontend/widgets/OBSBasic.hpp` — one `friend` declaration.
- `frontend/cmake/ui-widgets.cmake` — register new sources.
- `CHANGES.md`.

No libobs edits.

---

## 8. Risks / open points

1. **FTB does not hide overlays** (channel 0 only). Cut/Fade/T-bar also do not,
   as required. If you want FTB to black the whole output, Mixish must hide all
   overlay items while FTB is active — one extra hook, easy, but changes the
   "always on top" rule. **Please confirm which behaviour you want.**
2. Channels 16–19 are a convention, not a reservation; if a future OBS version
   starts using them, one constant moves overlays elsewhere. Low risk.
3. Overlay audio is audible through the Program mix but has no separate mixer
   strip; the input's strip still controls its volume. An overlay-only mute is
   available through the OV controls.
4. Fades depend on `obs-transitions` being loaded. If `fade_transition` is
   missing, the channel falls back to Cut and logs it.
5. Windows-only verification, as with the rest of Mixish.
6. The canvas editor renders a wireframe plus an optional live thumbnail; it is
   not a full interactive program monitor yet.

---

## 9. Build stages (each ends with a build, a run, screenshots, CHANGES.md)

- **(a)** Controller + channel 0 of the overlay set + strip button 1; ON/OFF
  with Cut; verify Program/stream/record.
- **(b)** All four channels + ascending layering + All Off.
- **(c)** Per-channel Fade in/out with separate durations.
- **(d)** Position / size / crop + `MixishCanvasEditor`.
- **(e)** Tile bottom row, strip states, Overlay panel, bottom bar.
- **(f)** Hotkeys, preview indicator, save/restore across restart and scene
  collection switch.
