# Mixish 0.0.2

Released 2026-11-16 · [All changes](CHANGES.md)

Second public build of Mixish, a live production switcher built on OBS Studio.
This release adds the Playlist input, fixes media playback during transitions,
and gives the app a proper startup splash.

## Highlights

### Playlist input
- New **Playlist** source type in Add Input, directly below Media.
- Build a queue from multiple files or an `.m3u` / `.m3u8` list. `#EXTINF`
  titles and durations are read from the list; unknown durations are probed in
  the background and cached with the scene collection.
- New **Playlist** tab next to Audio Mixer: item list with availability and
  duration, Add / Edit / Remove, Play Out, Auto Next, Auto First and Loop.
- Missing files are skipped, the last frame is held at the end of the queue,
  queue order / current item / durations survive a restart.
- Playback uses a native OBS media source; no VLC or extra runtime is required.

### Program transport
- The transport widget under Program shows the active Playlist position and the
  next item, and drives previous/next and play/pause on Program.
- The INPUTS transport and the Program transport are one shared widget
  (`MixishMediaTransport`), not two implementations.

### Media playback fixed for transitions
- A media input no longer pauses or deactivates when a Fade takes it off
  Program; it keeps playing until the fade has finished.
- Coming back on air resumes from the held frame, never from 00:00.
- Selecting a media input in Preview does not start playback.
- Autoplay that OBS triggers while loading a scene collection is parked once;
  an intentional play (transport, hotkey, websocket) is respected.
- New **Playback** group in Input Properties with the three vMix-style options:
  `Automatically play with transition` (on), `Automatically restart with
  transition` (off), `Automatically pause after transition` (on). `Defaults`
  restores exactly those values. Settings persist with the scene collection.

### Startup splash
- 480×300 Mixish splash with wordmark, tagline, accent progress sweep, live
  status and the app version, drawn from the Mixish design tokens.
- Shown before the main window exists, stays at least 1.1 s, fades out in 220 ms
  once the main window is on screen.

### Input and Add Input fixes
- **Del** on an input now also removes the auto-created empty scene behind it,
  so the tile disappears instead of leaving a stale empty scene.
- Playlist sits directly below Media in the Add Input type list.

### Stability
- Fixed an OBS `ThumbnailManager` crash when a source was destroyed while its
  UUID was still queued for a thumbnail update.

## Known limitations

- Playlist switching is not gapless yet: v1 replaces the file in one media
  source (measured 1.9–19.4 ms gaps on the test machine). Dual-source
  preloading is planned for v2.
- `.m3u` / `.m3u8` files are treated as Mixish queue lists; a true HLS playlist
  is expanded into its segment entries rather than streamed as one item.
- The installer is not code-signed, so Windows SmartScreen may show
  "Unknown publisher".
- Settings still live in OBS's `%APPDATA%\obs-studio` folder.

## Install

Download `Mixish-Setup-0.0.2-x64.exe` below and run it. A SHA-256 file for the
installer is published next to it. Uninstalling leaves user data in place.

## Verification

- Build: RelWithDebInfo x64, `obs-studio.vcxproj`.
- Playlist, media semantics, playback options and the splash were verified at
  1366×768 with screenshots and native OBS media status; details are in
  `CHANGES.md`.
- 10× close test on this build (only `WM_CLOSE`, no mouse/keyboard automation):
  **10/10 clean exits, 0 new crash files**.

## License

Mixish is based on OBS Studio and is licensed under GPL-2.0-or-later. All
original OBS Studio copyright and license notices are kept; see `COPYING`.

---

## สรุปภาษาไทย

- เพิ่มอินพุต **Playlist** สร้างคิวจากไฟล์หลายไฟล์หรือ `.m3u`/`.m3u8`
  มีแท็บ Playlist ข้าง Audio Mixer (เพิ่ม/แก้ไข/ลบ, Play Out, Auto Next,
  Auto First, Loop) และคิวคงอยู่หลังรีสตาร์ท
- เพิ่มแถบควบคุมใต้จอ Program สำหรับ Playlist ที่ออกอากาศอยู่
- แก้พฤติกรรมมีเดียตอนเปลี่ยนฉาก: ไม่ตัด/ไม่หยุดกลาง Fade, กลับมาออกอากาศต่อ
  จากเฟรมเดิม, ไม่เล่นเองใน Preview และมีตัวเลือกแบบ vMix 3 ข้อในหน้า Properties
- เพิ่มหน้า **Splash** สวย ๆ ตอนเปิดโปรแกรม
- ปุ่ม Del ลบซีนว่างที่สร้างอัตโนมัติให้ด้วย และแก้ crash ของ ThumbnailManager
- ข้อจำกัด: Playlist ยังเปลี่ยนไฟล์แบบไม่มีช่องว่างไม่ได้ (1.9–19.4 ms),
  ตัวติดตั้งยังไม่เซ็นชื่อ (SmartScreen อาจเตือน)
