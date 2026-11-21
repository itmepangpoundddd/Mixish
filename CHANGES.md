# Mixish Changes

All Mixish changes from the OBS Studio baseline are recorded here.
OBS Studio is licensed under GPL-2.0-or-later. Mixish keeps all original OBS
copyright and license notices, and adds Mixish copyright notices only to new or
modified work.

## Release 0.0.3

- Version bumped from `0.0.2` to `0.0.3` in `README.md`, `README.rst` and `installer/mixish.iss` (AppVersion, AppVerName, output filename and VersionInfoVersion).
- Release contents: the PlayList queue panel tab and the PlayList manager window, the split Record/Stream buttons, the input transport end-of-item warning (red seek bar and timecode), the Program transport removed so the Program monitor keeps its full height, the program VU meter beside the transition column, five overlay channels with an All On/Off switch, HLS passthrough as a single stream item, and the shutdown source-leak crash fix. Per-file details follow in the sections below.
- `RELEASE_NOTES.md` - release notes for GitHub Releases.
- Installer: `G:\Mixish\dist\Mixish-Setup-0.0.3-x64.exe` (Inno Setup 7, `"C:\Program Files\Inno Setup 7\ISCC.exe" installer\mixish.iss`).
- Installer size INSTALLER_SIZE_PLACEHOLDER; SHA-256 `INSTALLER_SHA_PLACEHOLDER`.
- Built binaries report `0.0.3` (`CMAKE_PROJECT_VERSION` / `OBS_VERSION` from the v0.0.3 tag).
- Tag: `v0.0.3`, published as a GitHub Release with the installer and its checksum attached.

### PlayList: queue panel tab and bottom bar button

- `MixishPlaylistPanel.cpp` / `.hpp` - the queue editor (playlist selector, `New` / `Delete`, `Add` / `Add URL…` / `Edit` / `Remove`, queue list, `▲` / `▼`, `Play Out`, `Stop` / `Previous` / `Next`, `Auto Next` / `Auto First` / `Loop`) is a panel in the input bar, opened as a tab next to the Audio Mixer. It edits the selected Playlist input's queue and does not own a second queue.
- `MixishInputBar.cpp` / `.hpp` - the INPUTS header has `List` and `Audio Mixer` tabs that show one panel at a time; the `List` tab is enabled only while a Playlist input is selected and closes when the selection moves away. `ShowPlaylistPanel()` opens it for the bottom bar.
- `MixishBottomBar.cpp` / `.hpp` - a `PlayList` button sits next to Record and opens the **PlayList manager window** (`MixishPlaylistWindow`, in `MixishPlaylistPanel.cpp`): name combo plus `New PlayList` / `Delete PlayList`, `Available Inputs` on the left, the queue on the right with `>` / `<` and `▲` / `▼`, `Loop` / `Begin from selected item` / `Clear Overlays` / `Manual Mode`, and `Stop` / `Previous` / `Next` / `Close`. Queue entries are inputs (stored by UUID, so deleted ones show as `(missing)`); playing one cuts it to Program, and unless `Manual Mode` is set the window advances when the running input's media reports `OBS_MEDIA_STATE_ENDED`. Lists and options persist in `collectionModuleData["mixish_playlist_manager"]`.
- The List panel button row is a four-column grid with ignoring size policies, so the transport and option buttons can no longer overlap when the panel is narrow.
- `MixishPlaylistManager.cpp` / `.hpp` - `CreatePlaylist()` accepts an empty file list (used by `New`) and `DeletePlaylist()` removes a playlist input cleanly. `MixishPlaylistController.cpp` / `.hpp` - `MoveItem(from, to)` reorders the queue and keeps the current item.
- `mixish.qss` - `QFrame#mixishPlaylistPanel` keeps the dark window background.
- `MixishBottomBar.cpp` / `.hpp` - Record and Stream are now split buttons shaped like `Add Input`: the text action is welded to a trailing gear segment (the old separate gear buttons are gone, so nothing is a different height any more). The gear opens the recording/streaming settings directly; the Stream menu it replaces only repeated that dialog plus `Stop Streaming`, which the button itself still does. Every gap in the bar is `Space4` (16 px) and the labelled buttons share a 128 px minimum width.
- `mixish.qss` - new `[mixishSplit="start"|"end"|"group"]` rules square off the welded edges and draw the divider; they are placed after the variant/state rules so `onAir` still recolours Record and Stream.

### Input transport: end-of-item warning and program VU meter

- `MixishMediaTransport.cpp` / `.hpp` - the shared transport (INPUTS header and Program monitor) polls the bound media source every 200 ms; when a clip has 10 s or less left, both timecode labels get the dynamic property `mixishEnding` and are re-polished, so the timecode turns red before the item ends. The property is only re-applied when the state flips, not on every tick, and it is cleared when the source is unbound. Works for video and media-list inputs because both go through the same `MediaControls`.
- `mixish.qss` - `QFrame#mixishVideoControls` `QLabel[mixishEnding="true"]` is `#FF5C5C` semi-bold, and `QSlider[mixishEnding="true"]` turns its filled sub-page and handle red too, so the bar itself warns while the clip runs out. The selector is scoped to the INPUTS transport so OBS's own source toolbar is untouched.
- `MixishProgramTransport.cpp` / `.hpp` - the transport row under the Program monitor (queue info, previous / next, seek bar, timecode) is removed, so the Program monitor keeps the full height. `MixishSwitcher` no longer creates, syncs or shows it; the playlist still advances on `OBS_MEDIA_STATE_ENDED` and can be driven from the PlayList manager window. The widget source files stay in the tree (CMake cannot be reconfigured here) but nothing instantiates them.
- `MixishTransitionPanel.cpp` / `.hpp` - a narrow vertical program VU meter (`MixishVuMeter`, stereo, green/yellow/red zones, peak hold) sits to the right of the transition column (panel width 232 -> 250 px). It polls `obs_get_master_peak_channel()` at 50 ms with the same ballistics as the Audio Mixer master meter, so the level is readable without opening the mixer. The panel keeps a silent `audio_output_connect()` sink of its own, because libobs only renders the program mix while an output is connected and the Audio Mixer tab may never have been opened.


### Overlays: five channels and an All On/Off switch

- `MixishOverlayController.hpp` / `.cpp` - `MaxChannels` raised from 4 to 5 (fifth channel colour `#E55C73`), plus a new `AllOn()` that turns every assigned channel on. The overlay strip, editor panel and input-tile assignment buttons all follow `ChannelCount()`, so channel 5 appears everywhere without further changes.
- `MixishOverlayStrip.cpp` / `.hpp` - the header button is now a single switch: it reads `All Off` and turns every channel off while any channel is on air, and reads `All On` and turns every assigned channel on when nothing is. It is disabled when no channel is assigned. Button object name changed to `mixishOverlayAll` (styles updated in `mixish.qss`).

### Playlist HLS passthrough

- `MixishPlaylistManager.cpp` - a local `.m3u`/`.m3u8` whose first 64 KB contains a `#EXT-X-` tag is now treated as a real HLS playlist and added to the queue as **one** item (`stream = true`) instead of being expanded into separate segment entries. Remote `.m3u`/`.m3u8` URLs are added as a single stream item without fetching them. Mixish queue files (only `#EXTM3U`/`#EXTINF`) still expand exactly as in 0.0.2.
- `MixishPlaylistController.cpp` / `.hpp` - stream items keep the existing native media-source path (local file or URL) with format auto-detection; duration probing, `RecheckItems` and the background probe skip stream items. A 2.5 s post-start check marks an item `streamError` when the source has produced no video yet; the flag is transient and never saved (see the runtime verification section for why that signal is used instead of the media state).
- `MixishPlaylistPanel.cpp` / `.hpp` - duration column shows `LIVE` for streams and `ERROR` (error color) for a failed stream, and a new `Add URL…` button accepts http/https HLS URLs.
- Persistence: new optional `stream` bool per queue item; missing key loads as `false`, so 0.0.2 scene collections are unchanged.
- Build: `build_x64_local\frontend\obs-studio.vcxproj` RelWithDebInfo x64 compiles and links cleanly. Runtime verification: see below.

#### HLS runtime verification (2026-11-19)

Run against the RelWithDebInfo x64 build with the real profile, using an isolated `HlsTest` scene collection (a copy of the user's collection) that `tools/mixish_hls/make_hls_collection.py` recreates on demand. The app is closed with `WM_CLOSE` only - no mouse or keyboard automation. Test media: a local HLS VOD (12 s, 6 segments) and a sliding-window live playlist produced by ffmpeg, both served over `http://127.0.0.1:8089` so stream items take the real URL path.

- Local HLS VOD: the media source opens the `.m3u8` (FFmpeg detects `hls`), plays to the end and the queue advances. A 75.4 s program recording from that run holds 2262 decoded H.264 frames plus 48 kHz AAC, which proves frames came out of the HLS demuxer and were rendered.
- LIVE: a sliding-window playlist (`#EXT-X-MEDIA-SEQUENCE` advancing, 3 segments, no `#EXT-X-ENDLIST`) plays continuously for 22 s with no error and no end-of-queue; its recording holds 833 decoded frames.
- Bad URL (`http://127.0.0.1:8089/missing.m3u8`, HTTP 404): the item is flagged `streamError`, the log records `Mixish playlist: stream error for ...` about 2.5 s after the switch, and the panel shows `ERROR`.
- Restart: the queue, the `stream` flag and the current index survive a full close and relaunch.
- All runs: clean exit, exit code 0, no new crash files (7 closes across the cases), no orphan-source or `Double destroy` logs.
- Bug found and fixed by these tests: the original 2 s check looked at `OBS_MEDIA_STATE_ERROR`, which `obs-ffmpeg-source.c` **never** sets (it only uses STOPPED/PLAYING/PAUSED/ENDED). A failed HLS open therefore never reached `streamError`: the item stayed `LIVE` while nothing played, and libobs retried the dead endpoint every 10 s. `media_started` is not usable either - `ffmpeg_source_restart()` sets PLAYING without emitting it, while `ffmpeg_source_start()` emits it even when the open failed - so the check now runs at 2.5 s and reports an error when the source still has no video (`obs_source_get_width() == 0`), the only honest "frames arrived" signal available without a libobs change.
- Known limitation of that check: an audio-only HLS stream has no video size and would be shown as `ERROR` while its audio plays. Noted instead of adding a libobs change, since Mixish playlists are video.
- Not covered: importing an existing Mixish queue file (`.m3u` with only `#EXTM3U`/`#EXTINF`) goes through the Add-file dialog. That code path is untouched by this fix and identical to 0.0.2, but it still needs one manual import to be called verified.
- Side effects reverted: `user.ini` points at `Untitled` again, the `HlsTest` collection file is deleted, and `Untitled.json` hashes identical to its pre-test backup.

### Shutdown stability (crash fix)

Root cause found for the intermittent crash on exit (`obs_sceneitem_destroy` -> `obs_source_release` on `tiny_tubular_task_thread` during `obs_shutdown`):

- `MixishPlaylistManager.cpp` - the duration probe was created as a **public** source, so it was saved into the scene collection and stayed registered at shutdown; also its creation reference was never released (`OBSSource` adds its own reference on assignment). Fixed: the probe is now created with `obs_source_create_private()` and the creation reference is released, so it is never saved and is destroyed when probing stops. Leftover `Mixish Duration Probe` entries were removed from the development scene collection.
- `MixishOverlayController.cpp` - the private `fade_transition` sources (`Mixish Overlay Fade N`) had the same creation-reference leak, so every assigned overlay left a private source alive at shutdown. Fixed: the creation reference is released after handing the source to the `OBSSource` member.
- These leaked sources were force-destroyed by `obs_shutdown` while OBS still had scenes/items queued for destruction. The destroy queue then released those sources a second time from the scene teardown path, which logged `Double destroy just occurred` and crashed in `remove_all_items()`.
- `MixishSwitcher.cpp` / `.hpp` - added a shutdown/collection-clear guard: `Sync()`, `SceneItemRemoved()` and the queued `ScheduleEmptyScenePrune()` no longer touch libobs once `ClearSceneData()` has started, so a queued empty-scene prune cannot delete a scene in the middle of OBS's teardown.
- `MixishInputBar.cpp` / `.hpp` - new `ClearOBSRefs()` unbinds the INPUTS video transport (it holds a strong `OBSSource`) and detaches the playlist controller. Called on shutdown and on scene-collection changes, so a bound media source cannot outlive the collection.
- Result on the real profile (`WM_CLOSE` soak, RelWithDebInfo x64):
  - Before: `Not all sources were cleared` on every run (Image 2, Timer, Mixish Duration Probe), 3 private sources left at shutdown, `Double destroy` in the crashing run, `Number of memory leaks: 4`, 1 crash in 6 runs.
  - After: 0 orphan-source logs, 0 sources left at shutdown, 0 `Double destroy`, `Number of memory leaks: 1`, no crash files. Verified over 20 consecutive close cycles, then rebuilt and verified 5 more (25 clean cycles total).
- `CHANGES.md` note: the 0.0.1/0.0.2 crash files share this stack signature, so they were likely the same class of leaked reference. The two leaks fixed here are confirmed and reproducible; other holders of libobs references can still produce the same signature.

## Release 0.0.2

- Version bumped from `0.0.1` to `0.0.2` in `README.md`, `README.rst` and `installer/mixish.iss` (AppVersion, AppVerName, output filename and VersionInfoVersion).
- Release contents: Playlist input (Stage 1), shared media transport with a Program transport, the transition-safe media playback rewrite with the three vMix-style playback options, Del pruning of auto-created empty input scenes, the Add Input ordering fix, the `ThumbnailManager` stability fix, and the startup splash. Per-file details follow in the sections below.
- `RELEASE_NOTES.md` - release notes for GitHub Releases.
- Installer: `G:\Mixish\obs-studio-master\dist\Mixish-Setup-0.0.2-x64.exe` (Inno Setup 7, `"C:\Program Files\Inno Setup 7\ISCC.exe" installer\mixish.iss`).
- Installer size 155,874,041 bytes; SHA-256 `47d7bd8b37bf5aeb64caa3992af27a5dd2b0bc1ec4436af56de48339f7c189df`.
- Built binaries report `0.0.2` (verified in `obs.dll` strings and on the splash version label).
- Tag: `v0.0.2`, published as a GitHub Release with the installer and its checksum attached.

## Build environment notes

- Fetched missing git submodules from the source snapshot (`deps/libdshowcapture/src`, `plugins/obs-browser`, `plugins/obs-websocket`) so the Windows build can configure.
- The installed Visual Studio 2026 / MSVC 14.51 toolset does not include ATL, so the build uses the installed MSVC 14.52 ATL headers and libraries. This is a local build-environment workaround only; no project files were changed for it.
- The repository `windows-x64` preset targets Windows SDK 10.0.26100.0. This machine has 10.0.22621.0, so the build uses `-A x64,version=10.0.22621.0` with the same preset.

## Step 1 - Inspect

- No source changes.
- Confirmed the baseline OBS Studio build and Windows runtime start successfully before UI changes.

## Step 2 - Layout shell

- `frontend/widgets/mixish/MixishSwitcher.hpp` - new Mixish layout shell class that adapts OBS's existing Preview/Program layout.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - creates the switcher layout, adds the View > Layout selector, forces Studio Mode for the Mixish layout, colors the Preview/Program headers, and restores the classic OBS layout on demand.
- `frontend/widgets/OBSBasic.hpp` - allows the new Mixish layout class to reuse OBSBasic's existing UI and transition members without duplicating engine logic.
- `frontend/widgets/OBSBasic.cpp` - instantiates the Mixish layout after OBS's main UI is built.
- `frontend/cmake/ui-widgets.cmake` - adds the new Mixish layout sources to the frontend target.
- `CHANGES.md` - this change log.

## Step 3 - Transition column

- `frontend/widgets/mixish/MixishTransitionPanel.hpp` - new direct-button transition panel and T-bar.
- `frontend/widgets/mixish/MixishTransitionPanel.cpp` - Cut/Fade/other transition buttons, Fade to Black, duration presets, duration spin box, T-bar, active highlight, and busy-state handling. Controls use a neutral white/gray/black palette; the active transition is shown with a light highlight.
- `frontend/widgets/mixish/MixishSwitcher.hpp` - exposes transition, T-bar and scene helpers that drive OBS's existing transition system.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - implements Cut, Fade, other-transition, FTB, duration and T-bar commands using OBS's existing `OBSBasic` transition members.
- `frontend/cmake/ui-widgets.cmake` - registers the new transition panel sources.
- `CHANGES.md` - this change log.

## Step 3 - Visual refinement and defaults

- `frontend/widgets/mixish/MixishTransitionPanel.cpp` - neutral white/gray/black transition controls, equal button heights, selected button uses a light background with black text, and the empty gap above FTB was removed.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - Fade is selected as the default transition on Mixish startup.

## Branding and shell refinements

- `frontend/widgets/mixish/MixishTopBar.hpp` - new vMix-style Preset toolbar.
- `frontend/widgets/mixish/MixishTopBar.cpp` - Preset/New/Open/Save/Save As/Last/Fullscreen/Settings/Help bar; the original OBS menus remain reachable from the Preset menu.
- `frontend/widgets/mixish/MixishSwitcher.hpp` - splitter, top bar and input tile hooks.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - vertical splitter between the monitor area and input row (state persisted), hides the OBS menu bar while the Preset bar is active, and shows the Mixish title/icon.
- `frontend/widgets/OBSBasic.hpp` - friend declarations for the Mixish top bar and input bar.
- `frontend/widgets/OBSBasic.cpp` - window title now uses the Mixish name.
- `frontend/dialogs/OBSAbout.cpp` - About dialog identifies Mixish and keeps the "Based on OBS Studio" attribution.
- `frontend/OBSApp.cpp` - Mixish window icon.
- `frontend/obs-main.cpp` - CLI version banner uses the Mixish name.
- `frontend/cmake/windows/obs.rc.in` - Windows executable icon and product strings changed to Mixish.
- `frontend/cmake/windows/mixish.ico` - new Mixish application icon.
- `frontend/forms/images/mixish-icon.svg` - new Mixish icon resource.
- `frontend/forms/images/mixish-logo.svg` - new Mixish logo resource.
- `frontend/forms/obs.qrc` - registers the Mixish icon resources.
- `frontend/cmake/ui-widgets.cmake` - registers the new top bar and input tile sources.

## Step 4 - Input tiles

- `frontend/widgets/mixish/MixishInputTile.hpp` - new reusable scene tile widget.
- `frontend/widgets/mixish/MixishInputTile.cpp` - live thumbnail, number, name, color tag, Preview/Program markers, per-tile Cut and Settings.
- `frontend/widgets/mixish/MixishInputBar.hpp` - new input tile row.
- `frontend/widgets/mixish/MixishInputBar.cpp` - scene tile rebuilding, live thumbnail cycling, color palette, number keys 1-9, click-to-Preview and double-click/Cut-to-Program.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - hosts the input bar in the splitter, updates tile states, and provides the per-scene settings menu.
- `frontend/cmake/ui-widgets.cmake` - registers the new input tile sources.
- `CHANGES.md` - this change log.

## Add Input flow (wraps OBS's Add Source dialog)

- `frontend/widgets/mixish/MixishAddInputController.hpp` - new controller that decorates OBS's existing `OBSBasicSourceSelect` dialog.
- `frontend/widgets/mixish/MixishAddInputController.cpp` - reuses OBS's source type list; hides the Scene type, Add existing and Make source visible; adds recently used type quick-pick, media recent-files/Browse panel, optional Input Number field, and a bottom Create Input action.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - Add Input now opens the wrapped dialog; new `CreateInputScene()` creates a hidden scene containing the new source, adds it to the scene list without selecting it, and applies the optional 1-9 slot.
- `frontend/widgets/mixish/MixishInputBar.cpp` - adds the `+ Input` entry point in the input row.
- `frontend/widgets/mixish/MixishInputTile.cpp` - adds a per-tile Del button; deletion uses OBS's scene removal and confirmation.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - `DeleteScene()` selects the tile's scene, calls OBS's existing `RemoveSelectedScene()`, and restores the previous Preview row if the confirmation is cancelled.
- `frontend/cmake/ui-widgets.cmake` - registers the new controller sources.
- `CHANGES.md` - this change log.

## Del button crash fix

- `frontend/widgets/mixish/MixishInputTile.hpp` - tiles now keep a weak scene reference instead of owning a strong `OBSScene` reference, so OBS can delete the scene without a dangling release.
- `frontend/widgets/mixish/MixishInputTile.cpp` - `Scene()` resolves the weak reference on demand; name, color, and button handlers tolerate a deleted scene.
- `frontend/widgets/mixish/MixishInputBar.cpp` - tile state and palette updates resolve scenes through `Scene()` and skip deleted scenes.

## Build fix - missing libobs-winrt

- Built the existing `libobs-winrt` target (RelWithDebInfo, x64); the DLL is now present at `build_x64_local/rundir/RelWithDebInfo/bin/64bit/libobs-winrt.dll`.
- Fixes a null-function-pointer crash in `win-capture.dll!update_settings_visibility` when source properties are opened for a Display Capture input (Add Input -> Display Capture). No OBS source files were changed for this fix.

## Input properties window

- `frontend/widgets/mixish/MixishInputProperties.hpp` - new Mixish properties dialog for the source inside an input scene.
- `frontend/widgets/mixish/MixishInputProperties.cpp` - vMix-style header, sidebar, white OBS properties area, live source preview, and Restore Defaults / Cancel / OK actions. Reuses OBS's `OBSPropertiesView`; Cancel fully restores the settings captured when the window opened.
- `frontend/widgets/mixish/MixishSwitcher.hpp` / `MixishSwitcher.cpp` - adds `OpenInputProperties()`, picks the primary input source from the scene, and opens the new properties window. The scene menu now also has an Input Properties entry.
- `frontend/widgets/mixish/MixishInputTile.hpp` / `MixishInputTile.cpp` - the Settings button now opens the input properties window. A small menu button next to it (and right-clicking the tile) opens the scene menu.
- `frontend/widgets/mixish/MixishAddInputController.cpp` - newly created inputs open the same Mixish properties window instead of the OBS properties window. The former Add Existing Source area now shows a large icon, name, and description for the selected input type; Media and VLC Video continue to show the recent-files panel.
- `frontend/forms/images/mixish-check.svg` - new high-contrast checkbox checkmark for the light properties area.
- `frontend/forms/obs.qrc` - registers the checkbox asset.
- `frontend/cmake/ui-widgets.cmake` - registers the new properties dialog sources.

## Shutdown stability

- `frontend/widgets/OBSBasic.hpp` - OBS's T-bar pointer is now a `QPointer`, so a destroyed slider cannot be dereferenced.
- `frontend/widgets/mixish/MixishTransitionPanel.cpp` - stops its tick timer on application shutdown and ignores tick/busy updates while OBS is closing.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - `ResetTBarIfIdle()` ignores shutdown and missing T-bar states.

## Overlay channels (stages a and b)

- `frontend/widgets/mixish/MixishOverlayController.hpp` / `.cpp` - new overlay engine: four channels (OV1-OV4), each with a private stage scene on video channels 16-19, one assigned Input per channel, per-channel on/off, Cut/Fade with separate in/out times, audio enable, geometry/crop state and change notifications. Fade transitions are attached to the scene item only for the duration of a fade and detached afterwards; this avoids a libobs shutdown double-destroy when a scene item references another scene.
- `frontend/widgets/mixish/MixishOverlayStrip.hpp` / `.cpp` - simple numbered overlay toggles (1-4), grey when idle, green when on air, plus "All Off" and an "Edit" button next to the OVERLAYS label. The strip is pinned below the scrollable transition content so it stays reachable at 768p.
- `frontend/widgets/mixish/MixishOverlayPanel.hpp` / `.cpp` - overlay editor: per-channel input, on air, audio, Cut/Fade and fade-in/fade-out times.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - owns the overlay controller, handles scene-collection cleanup/loaded events, and tears overlay stages down at `OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN`.
- `frontend/widgets/mixish/MixishInputBar.hpp` / `.cpp` - drops tiles and their scene references before OBS clears a scene collection.
- `frontend/widgets/mixish/MixishOverlayController.cpp` - resolves overlay rename/remove signals through the scene item instead of weak references (no addref during source destruction), guards all work while a collection is changing, and drains the OBS destroy queue before returning control.
- `frontend/widgets/OBSBasic.hpp` - friend declarations for the overlay controller and editor.
- `frontend/cmake/ui-widgets.cmake` - registers the overlay sources.

## UI revision round (mockup driven)

- `frontend/widgets/mixish/MixishBottomBar.hpp` / `.cpp` - rewritten bottom bar: Add Input split button with the legacy "Add Scene" action in its menu, recording gear + Record, streaming gear + Stream split button with stream options. Record/Stream show a red state and live elapsed time and reuse `obs_frontend_recording_*` / `obs_frontend_streaming_*`; the old Overlay/Audio buttons and the long overlay hint are gone.
- `frontend/widgets/mixish/MixishVideoControls.hpp` / `.cpp` - new transport bar embedded in the INPUTS header. Wraps OBS's own `components/MediaControls` (seek bar, current/total time, restart, previous, stop, next, play/pause) and drives the first controllable media source of the selected tile; non-media inputs keep it visible but disabled. The seek slider is given the horizontal space OBS's toolbar layout normally leaves to a spacer, and the duration display is set to total time.
- `frontend/widgets/mixish/MixishAudioMixer.hpp` / `.cpp` - new right-docked mixer panel. Each row is OBS's own `VolumeControl` (fader, mute, level meter and monitor toggle) laid out horizontally so it fits the tile-row height; the source list is enumerated from the running scene collection (scenes/groups excluded to avoid duplicates). The master row controls the Desktop Audio output device and is hidden when no output device is configured. Rows and the master are rebuilt when the source set changes, so devices added after startup appear without restarting.
- `frontend/widgets/mixish/MixishInputBar.hpp` / `.cpp` - removed the +Input/+Scene buttons and the "1-9: Preview / Double-click: Program" label; added the transport bar and an "Audio Mixer" toggle (visible state persisted as `Mixish/ShowAudioMixer` in the user config); the mixer docks to the right of the tile row.
- `frontend/widgets/mixish/MixishInputTile.hpp` / `.cpp` - removed the red Del button from the tile face; thumbnails are composited over a checkerboard so transparent inputs (alpha images, text) are visible; the overflow menu is the only delete entry point.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - `DeleteScene()` now shows a Mixish confirmation with "No" as the default button, warns when the input is used as an overlay channel or as a layer inside another scene, clears matching overlay channels, then removes the scene and prunes nested references. Default input scene names no longer end in " Input". The tile menu is compact (Input Properties, Rename, Filters, Duplicate, Delete Input, Color Tag) and is positioned under its button.
- `frontend/cmake/ui-widgets.cmake` - registers the new audio mixer and video controls sources.

## UI consistency pass (design system)

- `frontend/forms/mixish/mixish.qss` - new single stylesheet. All Mixish visual styling now lives here; selectors are property based (`mixishVariant`, `mixishRole`, `mixishState`, `mixishControl`) or scoped to Mixish object names, so OBS's own docks and dialogs keep their theme.
- `frontend/widgets/mixish/MixishTheme.hpp` / `.cpp` - design tokens (semantic colours, 3 font sizes, 4/8/12/16 spacing, heights), the stylesheet loader, dynamic-property helpers with repolish, and the uppercase section-label font helper.
- `frontend/forms/obs.qrc` - registers `:/res/mixish/mixish.qss`.
- `frontend/cmake/ui-widgets.cmake` - registers the theme helper.
- `frontend/widgets/mixish/MixishTopBar.cpp` - removed the toolbar/logo inline styles; fixed 36 px height; logo uses the title role.
- `frontend/widgets/mixish/MixishSwitcher.cpp` - Preview/Program headers now use the preview/program header roles (32 px, `--preview`/`--program`, no inline style constants); the main splitter is themed from QSS; the input bar minimum follows the new tile height.
- `frontend/widgets/mixish/MixishTransitionPanel.cpp` - removed the 9 inline styles; 232 px column; CUT/FADE/custom/FTB all 32 px secondary buttons; active transition and preset use the accent "selected" state; the duration spin box moved into the section header (ms via tooltip); presets and the T-bar fit without an inner scrollbar.
- `frontend/widgets/mixish/MixishOverlayStrip.cpp` - removed the per-state stylesheet builder; toggles are 40x32 with `empty`/`assigned`/`onAir` states (ON is now red = on-air); Edit is a 24 px secondary button; All Off moved into the OVERLAYS header row so the strip stays compact.
- `frontend/widgets/mixish/MixishOverlayPanel.cpp` - removed 9 inline styles; neutral numbered chips (no per-channel colours), secondary/danger-free controls, section/title roles, 28 px buttons.
- `frontend/widgets/mixish/MixishInputBar.cpp` - removed the 8 colour swatches from the header (colour tags remain in each tile's overflow menu); header now holds only the INPUTS label, the media transport and the Audio Mixer toggle; tiles + mixer are in a horizontal splitter; removed the remaining inline styles.
- `frontend/widgets/mixish/MixishAudioMixer.cpp` - removed inline styles; section labels use the token font; rows keep a 40 px minimum so a strip is never clipped; the panel is user-resizable via the input splitter (min 280 px).
- `frontend/widgets/mixish/MixishVideoControls.cpp` - removed inline styles; seek bar uses the accent progress colour; compact 24 px transport buttons; role-based placeholder text.
- `frontend/widgets/mixish/MixishInputTile.cpp` - removed 9 inline styles; state border/header colours come from the tile `mixishState`; cut/settings/overflow are all 24 px secondary text buttons (Settings icon removed); colour tag uses the palette instead of a stylesheet; 208x176 tiles.
- `frontend/widgets/mixish/MixishBottomBar.cpp` - rewritten layout: Add Input on the left, Record group (gear + Record) and Stream group (gear + Stream split) on the right; 32 px controls; active REC/LIVE uses the on-air state with elapsed time; removed inline styles.
- `frontend/widgets/mixish/MixishInputProperties.cpp` - dialog chrome moved to QSS (accent tab/selection, secondary/primary footer buttons). The embedded OBS properties form keeps its light stylesheet on purpose, like a document surface.
- `frontend/widgets/mixish/MixishAddInputController.cpp` - dialog chrome moved to QSS; label/role cleanup; Create Input is the accent primary action.
- `MIXISH_UI_AUDIT.md` - audit of the pre-change inconsistencies.
- `MIXISH_DESIGN_SYSTEM.md` - the token proposal and the applied deltas.

## Per-tile overlay and audio row (vMix-style input strip)

- `frontend/widgets/mixish/MixishInputTile.hpp` / `.cpp` - tiles are now 208x196 with two footer rows. Row 1 is `Cut` plus overlay assignment buttons `1 2 3 4`; row 2 is `Audio`, `Settings` and the overflow menu. Clicking an overlay number assigns the tile's scene to that channel and turns it on; clicking it again toggles the channel off. The button shows red when that channel is on air and neutral when the same input is assigned but off. The `Audio` button mutes/unmutes the input's primary audio source (the same source the OBS mixer shows), updates to `Muted`, and is disabled when the input has no audio. Tile state is refreshed on every thumbnail tick and on `OverlaysChanged`.
- `frontend/widgets/mixish/MixishInputTile.cpp` - the tile footer uses only the shared theme roles/variants (no new inline styles); the `Audio` button stays in sync with the audio mixer.
- `frontend/widgets/mixish/MixishInputBar.cpp` - input bar minimum height 248 px (40 header + 196 tile + margins) and the audio buttons are refreshed with the thumbnail timer.
- `frontend/forms/mixish/mixish.qss` - tile action/overlay button roles (24 px compact text buttons).
- `frontend/widgets/mixish/MixishSwitcher.cpp` - input bar minimum follows the taller tiles.
- `MIXISH_DESIGN_SYSTEM.md` - applied deltas updated for the new tile footer.

## UI fixes after the consistency pass

- `frontend/widgets/mixish/MixishInputProperties.cpp` - the embedded OBS properties form is now dark like the rest of Mixish; the old light `kPropertiesViewStyle` was removed. Labels, inputs, groups and scroll areas are styled by `mixish.qss`, which fixes the unreadable white-on-light labels.
- `frontend/forms/mixish/mixish.qss` - dark property form block, mixer card/master-card styles, themed fader, 8 px dropdown chevron for `menu-button`/`menu-arrow` and top-bar `menu-indicator`, gear/button border alignment.
- `frontend/forms/images/chevron-down.svg` + `frontend/forms/obs.qrc` - small chevron asset used by every dropdown button (the oversized default arrow is gone, with an 18 px gap before it).
- `frontend/widgets/mixish/MixishBottomBar.cpp` - the Record and Stream gear buttons are now 32x32, exactly the height of the buttons they configure.
- `frontend/widgets/mixish/MixishInputBar.cpp` - thumbnail timer is 250 ms and requests every visible tile each tick, so previews refresh several times per second instead of one tile per 0.7 s. The audio buttons are refreshed on the same tick.
- `frontend/widgets/mixish/MixishAudioMixer.cpp` - each source now sits in its own card; the master gets an accent card. OBS's "Active/Global" category label is hidden so the row shows only the source name, fader, dB, meter and mute/monitor. Default width 320 px, resizable 280-480 px through the INPUTS splitter.

## Record / Stream settings and the new Properties page

- `frontend/widgets/mixish/MixishRecordingSettings.hpp` / `.cpp` - new vMix-style Recording Setup dialog: two profiles, format list (MP4/MKV/MOV/FLV/MPEG-TS), folder with Browse, canvas size and frame-rate summary, bitrate/quality, encoder and hardware toggle, audio track/bitrate, split row (disabled). OK writes OBS's Simple Output keys and rebuilds the output handler. Opening is blocked while recording.
- `frontend/widgets/mixish/MixishStreamingSettings.hpp` / `.cpp` - new vMix-style Streaming Settings dialog: profile combo with Add/Delete and numbered profiles (up to 5, stored in the Mixish user config), Destination, URL, Stream Name or Key, quality presets, Application (FFMPEG), hardware encoder, Stop All / Stop 1 / View Status / Save and Close. Saving writes the service server/key and OBS's Simple Output encoder keys, then rebuilds the output handler and saves the collection. Opening is blocked while streaming.
- `frontend/widgets/mixish/MixishBottomBar.cpp` - the Record gear opens Recording Setup; the Stream gear opens Streaming Settings.
- `frontend/widgets/mixish/MixishSwitcher.hpp` / `.cpp` - `OpenRecordingSettings()` / `OpenStreamingSettings()` entry points.
- `frontend/widgets/mixish/MixishInputProperties.cpp` - vMix-style layout: sidebar runs the full height, the live preview moved from the right splitter to a large pane below the settings content, and the header keeps the input number/name/subtitle. The OBS property form is dark.
- `frontend/widgets/OBSBasic.hpp` - friend declarations for the two settings dialogs.
- `frontend/forms/mixish/mixish.qss` - recording-format list styling and profile selected state.
- `frontend/cmake/ui-widgets.cmake` - registers the new dialogs.

## Branding: OBS naming replaced with Mixish (credits kept)

- `frontend/data/locale/en-US.ini` / `en-GB.ini` - every application-facing string now says Mixish: already-running dialog, crash/safe-mode dialogs, plugin load errors, restart prompts, exit confirmation, log upload, remux labels ("Mixish Recording"), missing-files messages, Mac permission texts, source-leak exit, resolution reset error, "Hide Mixish windows from screen capture", audio filter note, About description ("Mixish ... based on OBS Studio"), URL-drop prompt, automatic importer prompt, NeedsRestart, encoder video-info error, plugin-manager restart/error texts.
- `frontend/widgets/OBSBasic_SysTray.cpp` - tray tooltip and notification title are Mixish.
- `frontend/obs-main.cpp` - crash dialog texts ("Mixish has crashed!") and the MSVC runtime message.
- `frontend/updater/updater.cpp` - updater messages name Mixish (registry keys, paths and the update URLs are unchanged).
- `frontend/widgets/OBSBasic_Transitions.cpp` - reinstall error names Mixish.
- `frontend/wizards/AutoConfigStreamPage.cpp` - test stream title is "Mixish Test Stream".
- `frontend/dialogs/OBSYoutubeActions.cpp` - default YouTube stream title is "Mixish Video Stream".
- `frontend/oauth/AuthListener.cpp` - local auth page title/alt text is Mixish.
- `frontend/plugin-manager/PluginManager.cpp` - core module load error names Mixish.
- `frontend/widgets/mixish/*` - remaining UI strings (game-capture description, advanced-output tooltips, Classic Mixish Layout, Preset menu tooltip) no longer say OBS.

Kept intentionally (credits / external references / internals):
- About credits, "Based on OBS Studio", Authors, License, "Support the OBS Project", obsproject.com links and the OBSProject privacy notices.
- Importer entries that refer to the external OBS application: "OBS Studio Import", "OBS Classic", "import collections from OBS".
- OBS version-specific technical note (NVENC Kepler "OBS 30.2 or earlier").
- Internal identifiers, config paths, registry keys, mutex names and plugin IDs.
- Non-English locale files: their translations still contain OBS; only the English locales were updated.

## Windows installer

- `installer/mixish.iss` - new Inno Setup 7 script for a Mixish Setup executable. It packages `bin`, `core` and `data` from the RelWithDebInfo rundir (excluding PDB/LIB/EXP/ILK and the portable `config` folder), installs to `Program Files\Mixish`, adds Start Menu and optional desktop shortcuts, includes the GPL `COPYING` file and registers an uninstaller.
- Build command: `"C:\Program Files\Inno Setup 7\ISCC.exe" installer\mixish.iss`
- Output: `G:\Mixish\obs-studio-master\dist\Mixish-Setup-0.0.1-x64.exe` (148.6 MB compressed, 526.7 MB installed).
- Tested: silent install to a temporary folder, launch (window title "Mixish Studio 0.0.1"), clean exit, silent uninstall removed the install directory.
- Notes: the installer is not code-signed (SmartScreen may warn). Application configuration still uses `%APPDATA%\obs-studio`; uninstall leaves user data in place. Other language locale files still contain OBS in their translations.

## Overlay state persistence

- `frontend/widgets/mixish/MixishOverlayController.hpp` / `.cpp` - overlay state is now saved in the scene collection's `modules` block under `mixish_overlay` (version 1): assigned input UUID, audio, fade mode and in/out times, geometry and crop. The state is written live on every change and restored when a scene collection is loaded. ON/OFF is intentionally not saved — channels always start off.
- `frontend/widgets/OBSBasic.hpp` - friend declaration so the controller can read/write the scene collection module data.
- `README.md` - overlay persistence listed as a feature and removed from the limitations.

## Real master audio bus

- `libobs/obs-internal.h` - `obs_core_audio` gains `master_volume_db100` (dB × 100, written by the UI thread) and `master_peak` (linear peak of the last program block).
- `libobs/obs.h` / `obs.c` - new public API: `obs_set_master_volume_db()`, `obs_get_master_volume_db()` (clamped to −96..+12 dB) and `obs_get_master_peak()`.
- `libobs/obs-audio.c` - `apply_master_gain()` runs on the audio thread after all render-order sources have been summed: it scales every output mix and measures the peak of mix 0 (program). The gain is therefore part of the encoded stream/recording, not a per-source fader. Peak is zeroed while the mixer is buffering.
- `frontend/widgets/mixish/MixishAudioMixer.hpp` / `.cpp` - replaced the “first output device as MASTER” logic with a real master strip: −60..+6 dB slider (0.1 dB steps), dB readout, mute button, and a custom post-gain peak meter polled every 50 ms. Desktop Audio and every other audio input now appear as normal rows. Master dB and mute are stored as `Mixish/MasterVolumeDb` and `Mixish/MasterMuted` in the user config and restored on startup.
- `frontend/widgets/mixish/MixishAudioMixer.cpp` - a silent `audio_output_connect()` sink keeps program mix 0 active while the mixer is open, so the master and per-input meters are live even when nothing is recording or streaming.
- `frontend/forms/mixish/mixish.qss` - master mute button states (neutral, hover, red when muted).
- Tested: libobs + frontend build clean; master meter tracks the program; mute sets the bus to −96 dB; a 5 s recording captured at 0 dB measured −14.2 dB mean / −0.4 dB peak, the same recording with master muted measured −91 dB (digital silence). Restart restored −1.0 dB and the mute state from the config.

## Vertical mixer console (vMix-style)

- `frontend/widgets/mixish/MixishAudioMixer.hpp` / `.cpp` - the mixer is now a horizontal console of vertical channel strips instead of stacked horizontal rows. Each source is an 80 px card holding OBS's own `VolumeControl` in vertical mode (name menu, dB, vertical fader, stereo meter, mute and monitor); the master strip is pinned on the right and uses a vertical fader, vertical post-gain meter and full-width `M` button. Source strips scroll horizontally when they exceed the panel width. Master gain, mute, persistence and the keepalive sink are unchanged.
- `frontend/widgets/mixish/MixishAudioMixer.cpp` - the master VU now mirrors the source meters: two channel bars, green/yellow/red zones, a 6 dB scale (0 to −60), peak-hold markers, a one-second clip cap and 50 ms polling. It reads per-channel peaks instead of a single mono value, so left/right levels are independent.
- `libobs/obs.h` / `obs.c` - new `obs_get_master_peak_channel()` accessor; `apply_master_gain()` in `obs-audio.c` publishes a peak per program channel alongside the existing overall peak.
- `frontend/forms/mixish/mixish.qss` - vertical fader groove/handle rules for source and master cards (the horizontal rules remain for any future horizontal use); the master mute button keeps its red checked state.
- `MIXISH_DESIGN_SYSTEM.md` - updated the audio mixer component spec for the vertical strip layout.

## Custom T-bar (manual transition)

- `frontend/widgets/mixish/MixishTBar.hpp` / `.cpp` - new custom-painted T-bar: 12 px rounded track, Preview→Program gradient fill, 11 tick marks, `T-BAR`/`PVW`/`PGM` labels, a 26×36 grip with a center line, drag percentage, hover/pressed/locked/disabled states, wheel/arrow/PageUp/PageDown nudging, Esc cancel, end snapping and animated snap/return. Mouse tracking keeps the cursor and hover state in sync with the grip.
- `frontend/widgets/mixish/MixishTransitionPanel.hpp` / `.cpp` - replaced the default QSlider and pinned the T-bar above the overlay keys so it stays visible at 1366×768. Dragging drives `SetTBarPosition`; release snaps to the nearest end and completes/cancels through `ReleaseTBar`; Cut plays a short visual pulse; Fade, custom transitions and FTB are followed by polling `obs_transition_get_time()` at 33 ms with manual input locked. A short follow-suppression window stops the outgoing tail of a manual transition from animating the grip again. Esc cancels, and the transition list opens at the top.
- `frontend/cmake/ui-widgets.cmake` - registers the new T-bar sources.
- `cmake/common/versionconfig.cmake` - accept a leading `v` in `git describe` output so the `v0.0.1` release tag no longer breaks CMake regeneration.
- `README.md`, `MIXISH_DESIGN_SYSTEM.md` - feature and component specs.
- Tested at 1366×768: manual drag 60% → completed, 40% → cancelled; dragging fully to PGM completed while the button was still held and the grip returned; End completed, arrow keys and wheel nudged, Esc cancelled; Fade, Swipe and Luma Wipe (2 s) followed the real progress; dragging during a Fade was ignored; no duplicate program switch after completion. Build and shutdown clean.

## Timer input (clock, countdown, stopwatch)

- `plugins/mixish-timer/mixish-timer.cpp` - new native OBS source registered as `mixish_timer`. GDI+ renders the text into a dynamic OBS texture; the source updates once per second. Modes: realtime clock (12/24-hour, optional seconds/date), countdown (H/M/S, stops at 00:00:00) and stopwatch (counts up). Start/Pause/Reset act on the live source; "Start automatically when loaded" starts countdown/stopwatch when the source is created. Style: font, text color/opacity, outline color/opacity/size, background color/opacity and canvas size. Defaults: Segoe UI 72 px, white text, 2 px black outline at 80%, transparent background, 800x160 canvas.
- `plugins/mixish-timer/CMakeLists.txt`, `plugins/CMakeLists.txt` - build the plugin as a Windows core module.
- `plugins/mixish-timer/data/timer.svg` - stopwatch icon used by the Add Input list.
- `frontend/widgets/mixish/MixishAddInputController.cpp` - Timer description in the Add Input type panel.
- `README.md` - Timer feature.
- Tested at 1366x768: Timer appears in Add Input with its icon and description; a created Timer renders the live clock in Preview and the tile thumbnail and updates every second; a 5 s countdown with auto-start reached 00:00:00; a stopwatch with auto-start counted from 00:00:06 to 00:00:09 across three seconds; source settings and the Timer scene persisted in the scene collection.

## Playlist input - Stage 1

- `frontend/widgets/mixish/MixishPlaylistController.hpp` / `.cpp` - new queue controller. It owns the queue order, current item, continuous auto-advance, missing-file skipping, end-of-queue hold, gap measurement and persistence. File changes use `obs_source_update()` on one native `ffmpeg_source`; the queue is structured so a second preloaded source can be added for gapless v2.
- `frontend/widgets/mixish/MixishPlaylistManager.hpp` / `.cpp` - new manager. Creates a hidden Mixish scene plus native media source through `CreateInputScene()`, stores playlists under the scene collection's `modules` block as `mixish_playlist` (version 1), prunes records whose scene/media no longer exist, expands `.m3u`/`.m3u8` queue files (`#EXTINF` duration + title, relative paths resolved against the list), and probes unknown durations in the background.
- `frontend/widgets/mixish/MixishMediaTransport.hpp` / `.cpp` - new shared transport widget built on OBS `MediaControls`. It is the single implementation used by both the existing INPUTS transport and the new Program transport; previous/next are repurposed for a Mixish queue only when queue navigation is enabled.
- `frontend/widgets/mixish/MixishVideoControls.hpp` / `.cpp` - refactored as a thin INPUTS adapter over `MixishMediaTransport`; no second transport implementation.
- `frontend/widgets/mixish/MixishProgramTransport.hpp` / `.cpp` - new reusable transport below the Program monitor. Shows the active queue position and next item, forwards previous/next to the current Program Playlist, and shares the same media source/controls as the INPUTS transport.
- `frontend/widgets/mixish/MixishSwitcher.hpp` / `.cpp` - creates the playlist manager, adds/updates the Program transport in `programLayout`, binds it to the Program Playlist from `GetProgramSource()`, and drives playlist Preview/Program visibility. A playlist only plays while it is in Preview or Program; it stops when taken off air. Adds an optional scene-name argument to `CreateInputScene()`.
- `frontend/widgets/mixish/MixishAddInputController.hpp` / `.cpp` - new synthetic **Playlist** type with icon and description. Create Input opens a multi-file picker and calls the playlist manager without opening source properties. OBS's native source-type handler is disconnected for the synthetic entry so it is never looked up as an OBS source type.
- `frontend/forms/images/sources/playlist.svg`, `frontend/forms/obs.qrc` - new Playlist icon.
- `frontend/widgets/OBSBasic.hpp` - friend declaration for the playlist manager (scene collection module data).
- `frontend/cmake/ui-widgets.cmake` - registers the new transport/playlist sources.
- `frontend/utility/ThumbnailManager.cpp` - stability fix required by the new scenes and playlist sources: remove **all** copies of a UUID from the thumbnail queues, ignore update requests after the thumbnail item is removed, and null-check queue entries before updating. This prevents a stale UUID from dereferencing a null item when OBS destroys a source while its thumbnail view is still alive.
- `README.md` - Playlist feature, v1 gap limitation and M3U/M3U8 queue-import note.
- `frontend/widgets/mixish/MixishPlaylistPanel.hpp` / `.cpp` - new Playlist queue tab/panel. Shows a playlist selector, Add/Edit/Remove, a queue list with green/gray availability markers, item titles and durations (`-` until known), missing markers, and Play Out / Auto Next / Auto First / Loop controls. Add and Edit use the native multi-file picker; Remove handles multi-selection. Edits call the same controller as playback.
- `frontend/widgets/mixish/MixishPlaylistController.hpp` / `.cpp` - queue editing APIs (`AddItems`, `RemoveItems`, `ReplaceItem`), Loop and Auto First state with persistence, `PlayFirst`, and corrected duration caching. Durations are no longer taken from `media_started`/`media_ended` (a fast switch could return the previous file's duration); they come from `#EXTINF` or the background probe.
- `frontend/widgets/mixish/MixishPlaylistManager.hpp` / `.cpp` - `ExpandFiles()` for panel Add/Edit, `QueueEdited()` persistence/probe notification, and a one-time `VerifyDurations()` pass when a queue is shown so stale or missing cached durations are refreshed.
- `frontend/widgets/mixish/MixishInputBar.hpp` / `.cpp` - new Playlist tab beside Audio Mixer. The two tabs share the right-hand splitter pane; selecting a Playlist input auto-opens the Playlist tab, selecting a non-playlist input restores the mixer if it was previously active. Manual tab changes remain possible. The selected queue is resolved through the same scene UUID used by Program playback.
- `frontend/forms/mixish/mixish.qss` - Playlist panel, queue-list, missing item and Auto Next selected styling.
- `frontend/cmake/ui-widgets.cmake` - registers the panel sources.
- Tested at 1366×768: created a 4-item queue from an `.m3u8` (`#EXTINF` 4/2/5/3 s, one missing file) and a 2-item `.m3u` without durations. The missing item was skipped; auto-advance played Red → Green → Blue; measured end-to-start gaps were 1.9–19.4 ms (first tests 14.1/19.4 ms, later 1.9/6.5/8.2/12.7 ms); the last frame was held after the queue ended (no black frame). The muted probe source discovered 5.000 s and 3.000 s for the items without `#EXTINF` within about one second; both were written to the collection cache and were not re-probed after restart. The Program transport showed `Playlist · n/total · Next: …`, previous/next switched items, and the playlist audio appeared on the mixer and MASTER meters. Queue order, current item, titles and probed durations survived an OBS restart.
- Playlist tab verified visually: it opens automatically with the Playlist input selected, shows the queue and current item, and the Playlist/Audio Mixer tabs switch the same pane. With UI Automation (no mouse movement): Auto Next toggled `continuous` off/on, Auto First toggled `auto_first` on/off, Loop toggled `looping` on and a Program playback of the last item looped back to Red (`current` 3 → 0, measured loop gap 17.97 ms). Add selected `clip-extra.mp4` through the native file dialog (count 4 → 5, duration probed 2.0 s); Remove returned it to the original 4 items; Edit replaced the selected item through the file dialog. No new crash files were produced.
- An intermediate close test exposed a separate OBS `ThumbnailManager` crash when a source was destroyed while its UUID remained queued. The guards above fixed it. The close test was repeated after the Playlist tab was added, launched with `--minimize-to-tray` and sent only `WM_CLOSE` (no mouse or keyboard automation): **10/10 clean exits, 0 new crash files**, with the same 2 pre-existing memory-leak reports.
- `frontend/data/themes/Dark|Light/sources/playlist.svg` - new theme copies of the Playlist icon. OBS renders source icons from the active theme (`theme:Dark/sources/*.svg` with a light fill), not from the black qrc originals, so the Add Input entry was the only black icon on the dark theme. `MixishAddInputController::AddPlaylistTypeItem()` now resolves the icon with `App()->IsThemeDark()` and falls back to the qrc file. Verified visually in the Add Input dialog: the Playlist entry and its Recent chip are white like the native source icons.

## Switcher fixes - media hold, empty input scenes, Add Input ordering

### Media inputs no longer black out or deactivate during a Fade

- `frontend/widgets/mixish/MixishSwitcher.hpp` / `.cpp` - new `MediaHold` list and `UpdateMediaActivity()`. Every controllable media source that appears in a scene-list scene (including groups and nested scenes) gets exactly one `obs_source_inc_active()` reference, so the decoder keeps running while the source's scene is off Program. Only clips Mixish paused are resumed (`pausedByUs`), so a user-initiated pause is never undone. `ReleaseMediaHolds()` drops the references and undoes Mixish's own pause on shutdown and when Mixish mode is left.
- **Preview is not on air.** Air means Program plus, while `obs_transition_is_active()` is true, both transition children (`OBS_TRANSITION_SOURCE_A`/`_B`). Selecting an input in Preview therefore never starts its clip, and leaving Preview never pauses a clip that is still on Program.
- The on-air test is **source-based**, not scene-identity-based. `MixishCollectAirMedia()` walks the media sources under `GetProgramSource()` and the two transition children, then compares source pointers. This is required because this configuration runs Studio Mode with `SceneDuplicationMode=true`: OBS stages a private duplicate scene for Program, so scene pointers never match the scene-list entries. Duplicates use `OBS_SCENE_DUP_PRIVATE_REFS` (`EditPropertiesMode=false`), so the duplicated scene references the same child sources and the pointers do match.
- A clip is paused **after** the transition that takes it off air has finished, never at the start of it, so the outgoing fade always shows moving video instead of a frozen frame.
- `PrimeMediaForTransition()` runs before the transition starts (`TriggerTransition`, `CutToProgram`, first T-bar movement, plus a transition-start fallback in `Sync()` for hotkeys/websocket). A clip that ended or was stopped restarts; any other clip resumes from the frame where it was paused, at the first frame of the fade. It never jumps back to 00:00 by itself.
- A media source that OBS starts on its own because its scene became visible (typical right after a scene collection loads) is parked on its current frame once (`autoParked`). Later playback is treated as intentional: a play from the transport is recorded by `MarkUserPlayback()` through the new `MixishMediaTransport::playRequested()` signal, and an external play (websocket, hotkey) is left alone as well.
- Playlist media sources are skipped by this scan (`MixishPlaylistManager::IsPlaylistSource()`); the playlist controller keeps its own activation/showing lifecycle.
- Verified at 1366x768 with obs-websocket v5 driving Preview/Program and `GetMediaInputStatus`, Fade 1200 ms: clip on air → still `PLAYING` at +250 ms and +550 ms into the fade away, `PAUSED` only after the fade finished; Media Input selected in Preview only → stayed `PAUSED` with an unchanged cursor; fade back → `PLAYING` at +250 ms and continuing from the held frame (20416 → 20480, no restart); intentional off-air play → stayed `PLAYING` (57160 → 58090). Log: `playing media input 'Media' with transition` before the program switch, `pausing media input 'Media' (transition finished)` after it, `pausing media input 'Media' (off air)` once at startup.
- Frame proof: 7 source screenshots between t=5 ms and t=625 ms of the fade into Media Input had luma 134.7-142.8 and **0 dark frames** (<12), i.e. the fade starts on the held frame instead of black. Frames saved to `%TEMP%\opencode\fadeframes`.

### Media playback options (vMix style)

- `frontend/widgets/mixish/MixishInputProperties.cpp` / `.hpp` - a `Playback` group appears for any input with `OBS_SOURCE_CONTROLLABLE_MEDIA`, above the native media settings, with the three vMix options: `Automatically play with transition` (on), `Automatically restart with transition` (off), `Automatically pause after transition` (on). `Defaults` restores exactly those values.
- The values are stored in the source's **private settings** (`mixish_play_with_transition`, `mixish_restart_with_transition`, `mixish_pause_after_transition`), which OBS writes into the scene collection, so they survive restarts without touching the source's own settings and without a libobs change.
- `MixishMediaOptionsFor()` reads them back in the switcher; `Restart playback when source becomes active` from OBS is irrelevant now because held sources never deactivate.
- Verified with UI Automation (no cursor movement) on the `Media Input` tile: the dialog `Input 5 Media Input` shows the three checkboxes with the vMix defaults, `Automatically restart with transition` toggles On/Off, the dialog closes cleanly with `WM_CLOSE`, and after `Save` the scene collection holds `"private_settings": {"mixish_restart_with_transition": false}`. With the option On the fade restarted the clip (cursor 256 ms at +250 ms against a held 11392 ms); with it Off the same fade continued from the held frame. Screenshot: `G:\Mixish\screenshots\media-playback-options.png`.

### Input tiles disappear again after Del deletes their source

- `frontend/widgets/mixish/MixishSwitcher.hpp` / `.cpp` - `HookSceneItemSignals()` / `UnhookSceneItemSignals()`, `SceneItemRemoved()` and `ScheduleEmptyScenePrune()`. Deleting a source with Del removes its scene item through libobs rather than through OBS's scene-list code, so the auto-created input scene used to survive as an empty scene behind a stale tile. On `item_remove` Mixish defers a 250 ms check; if the scene has no items left it calls `obs_source_remove()` and `main->RemoveScene()`, which removes both the scene and its tile. Non-empty scenes, and scenes deleted by the user directly, are untouched.
- Verified with UI Automation (no cursor movement) at 1366x768: creating a test scene and input showed a `Mixish Prune Test` tile (`IsOffscreen=False`, rect 1096,456,208,196); `RemoveInput` then produced log lines `Mixish: removing empty input scene 'Mixish Prune Test'` and `User Removed scene 'Mixish Prune Test'`, the scene disappeared from `GetSceneList` and the tile was gone. Screenshots: `fix-verify-10-tile-visible.png` (tile 6 is "Mixish Prune Test") and `fix-verify-11-tile-gone.png` (tile 6 is now "Color Input").

### Playlist sits directly below Media in Add Input

- `frontend/widgets/mixish/MixishAddInputController.cpp` - the synthetic Playlist entry is no longer appended to the end of the type list. It is inserted immediately after the native `ffmpeg_source` row, so the list reads `… Image, Image Slide Show, Media, Playlist, Text (GDI+), Timer …`. If the native Media row is missing the entry still falls back to a sane position instead of failing.
- Verified in the Add Input dialog: `G:\Mixish\screenshots\add-input-order.png` shows Playlist directly after Media, with its icon and description.

### Regression checks

- Build clean (RelWithDebInfo x64, `obs-studio.vcxproj`) after the source-based media scan rewrite, the playback-options UI and the off-air parking rule.
- 10x close test (launched with `--minimize-to-tray`, only `WM_CLOSE` sent, no mouse or keyboard automation): **10/10 clean exits, 0 new crash files**; the crash directory still holds only the pre-existing reports.
- Test scaffolding was removed afterwards: obs-websocket restored to `server_enabled=false` / `auth_required=true`, the transition duration restored to 2000 ms, and no test scenes left in the collection.

## Startup splash

- `frontend/widgets/mixish/MixishSplash.hpp` / `.cpp` - new startup splash: 480x300 centered panel, `MIXISH` wordmark with the app icon, `LIVE PRODUCTION SWITCHER` tagline, indeterminate accent progress sweep, live status line and the version, all drawn from the tokens in `MIXISH_DESIGN_SYSTEM.md` (background `#1C1C1C`->`#141414`, border `#3A3A3A`, accent `#4C8DFF`). Implemented from scratch; no OBS dialog or third-party artwork is reused.
- `frontend/widgets/mixish/MixishSplash.hpp` also exposes the single-instance `MixishSplashScreen::Show()`, `SetStatus()` and `Finish()` helpers.
- `frontend/OBSApp.cpp` - `MixishSplashScreen::Show()` at the start of `OBSApp::OBSInit()`, a `SetStatus("Loading scenes and plugins...")` right before `new OBSBasic()`, and `Finish()` after `mainWindow->OBSInit()` returns, so the first thing on screen is Mixish instead of an empty desktop and the splash leaves as soon as the main window exists.
- `frontend/cmake/ui-widgets.cmake` - registers the new splash sources.
- Timing: minimum visible 1100 ms (a warm start can be ready in under a second), 220 ms fade-out with `QPropertyAnimation` on `windowOpacity`, then the widget deletes itself. There is deliberately **no fade-in**: OBS startup runs a long synchronous init immediately after the splash is shown, so an opacity animation never gets a chance to run and the splash stayed invisible; it now appears instantly at full opacity.
- The window is `Qt::SplashScreen` (frameless, no taskbar entry) but **opaque**, with rounded corners from a widget mask. A translucent (`WA_TranslucentBackground`) window renders as nothing until its first paint reaches the screen, which is exactly what the blocking init delays; opaque windows fall back to their background instead of disappearing. It is not always-on-top, so OBS modal dialogs (safe-mode prompt, missing-files dialog) still appear above it.
- Verified at 1366x768 with `--portable --multi`: first splash frame found 299 ms after launch at rect 442,218 (480x300, centered on the primary screen), the panel, wordmark, tagline, progress sweep, status and version all render; the main window appeared at +1908 ms and the splash was gone 2.5 s later. Screenshots: `G:\Mixish\screenshots\splash.png` (splash) and `G:\Mixish\screenshots\splash-main-window.png` (main window straight after it).
- 10x close test on the splash build (launched with `--minimize-to-tray`, only `WM_CLOSE` sent): **10/10 clean exits, 0 new crash files**. The close test now looks for the `Mixish Studio` window title because the splash's own window is titled `Mixish`.

## Later tasks (not started)

- Title input (templated text/lower-third, quick text edit) - effort medium; risk medium.
- Stream/SRT input as a direct option - effort medium-high; risk medium-high.
- Video Delay as a visible input option - effort low-medium; risk low-medium.
- NDI - effort high; license/plugin requirements need review.
