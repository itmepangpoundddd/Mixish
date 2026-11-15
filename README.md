# Mixish

**A live production switcher built on OBS Studio.**

Mixish turns OBS Studio into a hardware-style switcher: Preview/Program monitors,
scene tiles, a transition column, four overlay channels and a docked audio mixer,
while keeping the OBS engine (scenes, sources, transitions, encoding) underneath.

> **Status:** early development (`0.0.1`). Expect rough edges; not yet recommended
> for unattended live production.

## Features

- **Preview / Program layout** with colored monitor headers and keyboard/mouse switching.
- **Input tiles** with live thumbnails, color tags, Cut, per-tile overlay assignment
  buttons `1 2 3 4`, audio mute and an overflow menu.
- **Transition column** with Cut, Fade, Fade to Color, Stinger, FTB, duration
  presets and a scrollable list of OBS transitions. The custom T-bar shows a
  Preview-green → Program-red fill, PVW/PGM labels, a drag percentage and a
  large grip; it follows button transitions, snaps at the ends, nudges with the
  wheel or arrow keys, and cancels with Esc.
- **Four overlay channels (OV1-OV4)** rendered above Program, independent of
  Cut / Fade / T-bar. Per-channel assignment, on/off, audio and fade editor.
  Assignments, audio, fade and geometry persist per scene collection; channels
  always start off.
- **Audio mixer** docked next to the input tiles: vertical channel strips with
  per-input faders, meters, mute and monitor toggles, plus a MASTER strip pinned
  on the right. The master gain is applied after the program mix in libobs (so
  it affects recording/streaming), has a stereo post-gain VU meter with a dB
  scale, and its level and mute state persist across restarts.
- **Media transport** for the selected input (seek, play/pause, restart, previous/next).
- **Input Properties** dialog with a dark OBS property form and a large live preview.
- **Recording / Streaming settings** dialogs modeled after a hardware switcher.
- **One design system**: a single `mixish.qss` stylesheet with semantic colors,
  three text sizes and four button variants.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 or newer Build Tools with the C++ workload
- Windows SDK 10.0.22621 or newer
- CMake 3.30+
- Git
- OBS Studio build dependencies (see the upstream OBS build instructions; the
  dependency bundle is not committed to this repository)

## Build

The upstream OBS build documentation applies. A typical Windows build:

```powershell
cmake --preset windows-x64
cmake --build build_x64_local --config RelWithDebInfo --target obs-studio
```

Run the portable build:

```powershell
build_x64_local\rundir\RelWithDebInfo\bin\64bit\obs64.exe --portable
```

The upstream README is kept as `README.rst` for the original OBS build details.

## Installer

An Inno Setup 7 script is provided at `installer/mixish.iss`. It packages
`bin`, `core` and `data` from the RelWithDebInfo rundir, excluding PDBs and the
portable config folder, and creates Start Menu/desktop shortcuts and an uninstaller.

```powershell
& "C:\Program Files\Inno Setup 7\ISCC.exe" installer\mixish.iss
```

The build product is not committed; attach installers to GitHub Releases.

## Repository layout

| Path | Contents |
| --- | --- |
| `frontend/widgets/mixish/` | Mixish layout, tiles, overlay engine, mixer, settings dialogs |
| `frontend/forms/mixish/` | `mixish.qss` and the small chevron asset |
| `installer/` | Inno Setup script |
| `MIXISH_DESIGN_SYSTEM.md` | Design tokens and component rules |
| `MIXISH_UI_AUDIT.md` | UI audit that led to the design system |
| `MIXISH_OVERLAY_DESIGN.md` | Overlay architecture |
| `CHANGES.md` | Change log for every Mixish change |

## Current limitations

- Recording/streaming settings map to OBS **Simple Output**; advanced-output-only
  options (file splitting, independent recording bitrate) are not exposed.
- The installer is not code-signed, so Windows SmartScreen may show
  "Unknown publisher".
- Application data uses OBS's `%APPDATA%\obs-studio` folder.

## License

Mixish is free software licensed under the **GNU General Public License,
version 2 or later** (GPL-2.0-or-later), the same license as OBS Studio.
See `COPYING` and `LICENSE`.

Mixish is based on [OBS Studio](https://obsproject.com/). All original OBS
copyright and license notices are retained. Mixish is an independent project
and is not affiliated with or endorsed by the OBS Project.

## Credits

- OBS Studio and its contributors — the engine, UI foundations and build system
- Qt, FFmpeg and the other open-source projects bundled with OBS
- Everyone who tested the switcher UI and reported issues
