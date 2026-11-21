# Mixish 0.0.3

Released 2026-11-21 · [All changes](CHANGES.md)

Third public build of Mixish, a live production switcher built on OBS Studio.
This release turns the Playlist input into a real rundown: a queue panel next
to the Audio Mixer, a PlayList manager window on the bottom bar, and end-of-item
warning on the input transport. It also fixes the crash on exit that 0.0.1 and
0.0.2 could hit at shutdown.

## Highlights

### PlayList: rundown panel and manager window

- New **List** tab next to **Audio Mixer** in the INPUTS header. It edits the
  queue of the selected Playlist input: playlists, `Add` / `Add URL…` / `Edit` /
  `Remove`, reorder with `▲` / `▼`, `Play Out`, `Stop` / `Previous` / `Next`,
  `Auto Next` / `Auto First` / `Loop`. The tab is only enabled while a Playlist
  input is selected and closes when the selection moves away.
- New **PlayList** button on the bottom bar, next to Record, opens the PlayList
  manager window: a playlist combo with `New PlayList` / `Delete PlayList`,
  `Available Inputs` on the left, the queue on the right, `Loop` / `Begin from
  selected item` / `Manual Mode`, and the transport. Queue entries are inputs
  (stored by UUID); playing one cuts it to Program, and unless `Manual Mode` is
  set the manager advances when the running media reports the end.
- Lists and options persist with the scene collection.

### Input transport: end-of-item warning and program VU meter

- When a clip has 10 s or less left, the seek bar under the INPUT monitor and
  both timecodes turn red, so the operator sees the item running out.
- The transport row under the Program monitor is gone; the Program monitor
  keeps the full height. The queue still advances on end-of-item and is driven
  from the playout panel or the PlayList manager.
- A narrow vertical VU meter sits to the right of the transition column and
  follows the program mix at 50 ms, so the level is readable without opening
  the Audio Mixer.

### Overlays: five channels and an All On/Off switch

- A fifth overlay channel (`#E55C73`) joins the strip, editor panel and input
  tile assignment buttons everywhere.
- The strip header is now a single switch: `All Off` while any channel is on
  air, `All On` when nothing is.

### Playlist HLS passthrough

- A `.m3u` / `.m3u8` that contains an `#EXT-X-` tag is treated as a real HLS
  playlist and added as **one** item instead of being expanded into segment
  entries; remote HLS URLs can be added with `Add URL…`. The queue list shows
  `LIVE` for a stream and `ERROR` when it produced no frames after 2.5 s.
- Mixish queue files (only `#EXTM3U` / `#EXTINF`) still expand exactly as in
  0.0.2.

### Record and Stream buttons

- Record and Stream are split buttons shaped like `Add Input`: the text action
  is welded to a trailing gear that opens the recording / streaming settings
  directly.

### Stability

- Fixed the intermittent crash on exit caused by leaked private sources: the
  playlist duration probe and the overlay fade transitions are now private and
  release their creation reference, and the switcher stops touching libobs once
  teardown has started. 25 consecutive close cycles were clean, with 0 orphan
  sources and 0 `Double destroy` logs.

## Known limitations

- Playlist switching is still not gapless: v1 replaces the file in one media
  source (measured 1.9–19.4 ms gaps on the test machine).
- An audio-only HLS stream, which has no video size, is shown as `ERROR` while
  its audio plays.
- Importing an existing Mixish queue file (`.m3u` with only `#EXTM3U` /
  `#EXTINF`) still goes through the Add-file dialog and was not re-verified for
  0.0.3.
- The installer is not code-signed, so Windows SmartScreen may show
  "Unknown publisher".
- Settings still live in OBS's `%APPDATA%\obs-studio` folder.

## Install

Download `Mixish-Setup-0.0.3-x64.exe` below and run it. A SHA-256 file for the
installer is published next to it. Uninstalling leaves user data in place.
Installing over 0.0.2 keeps the existing scene collections and settings.

## Verification

- Build: RelWithDebInfo x64, `obs-studio.vcxproj`, configured with
  `OBS_VERSION_OVERRIDE=0.0.3`; the built `obs.dll` reports `0.0.3`.
- Playlist panel, PlayList manager window, split Record/Stream buttons, the
  end-of-item warning, the program VU meter, the fifth overlay channel and the
  All On/Off switch were checked in the running app; details are in
  `CHANGES.md`.
- HLS passthrough was verified against a local HLS VOD, a sliding-window LIVE
  playlist and a 404 URL, with a program recording proving decoded frames.
- 25× close test on this build (only `WM_CLOSE`, no mouse/keyboard automation):
  **25/25 clean exits, 0 new crash files**.

## License

Mixish is based on OBS Studio and is licensed under GPL-2.0-or-later. All
original OBS Studio copyright and license notices are kept; see `COPYING`.

---

## สรุปภาษาไทย

- แท็บ **List** ข้าง Audio Mixer สำหรับแก้คิวของ Playlist ที่เลือก และปุ่ม
  **PlayList** บนแถบล่างเปิดหน้าต่างจัดการคิว (สร้าง/ลบเพลย์ลิสต์, เพิ่ม/ลบ,
  จัดลำดับ, Loop, Manual Mode) — คิวคงอยู่หลังรีสตาร์ท
- แถบ seek ใต้จอ INPUT และ Timecode เปลี่ยนเป็น **สีแดง** เมื่อคลิปเหลือ ≤10 วิ
  และมีหลอด **VU แนวตั้ง** ข้างแถบ Transition ดูระดับเสียง Program ได้โดยไม่ต้อง
  เปิด Audio Mixer
- ลบแถบ transport ใต้จอ **PROGRAM** ออก จอ Program ได้ความสูงเต็ม
- Overlay เพิ่มช่องที่ 5 และปุ่มสลับ **All On / All Off** ในแถบ header
- `.m3u`/`.m3u8` ที่เป็น HLS จริงถูกเพิ่มเป็นรายการเดียว (LIVE / ERROR) และเพิ่ม
  URL ได้จากปุ่ม `Add URL…`
- ปุ่ม Record และ Stream เป็น split button มีเฟืองเปิดหน้าตั้งค่าตรง ๆ
- แก้ crash ตอนปิดโปรแกรมจาก source ที่หลุด reference (ทดสอบปิด 25 รอบ ไม่มี crash)
- ข้อจำกัด: Playlist ยังเปลี่ยนไฟล์แบบไร้ช่องว่างไม่ได้, HLS ที่มีแต่เสียงจะขึ้น
  ERROR, ตัวติดตั้งยังไม่เซ็นชื่อ (SmartScreen อาจเตือน)
