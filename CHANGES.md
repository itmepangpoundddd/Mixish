# Mixish Changes

All Mixish changes from the OBS Studio baseline are recorded here.
OBS Studio is licensed under GPL-2.0-or-later. Mixish keeps all original OBS
copyright and license notices, and adds Mixish copyright notices only to new or
modified work.

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

## Later tasks (not started)

- Title input (templated text/lower-third, quick text edit) - effort medium; risk medium.
- Playlist input - effort medium-high; risk medium.
- Stream/SRT input as a direct option - effort medium-high; risk medium-high.
- Video Delay as a visible input option - effort low-medium; risk low-medium.
- NDI - effort high; license/plugin requirements need review.
