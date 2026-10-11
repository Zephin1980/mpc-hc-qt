# MPC-HC for Linux: porting plan

Goal: the same window layout, menus, command names and default shortcuts as MPC-HC,
running on libraries every major distribution ships.

## What the Windows code base actually is

| Area | Size (lines) | Windows-bound? | Linux plan |
|---|---|---|---|
| `src/mpc-hc` (main app: MainFrm, dialogs, settings) | ~138k | MFC + Win32, DirectShow calls throughout (`MainFrm.cpp` alone is ~25k lines) | Rewrite UI in Qt6 Widgets, keep structure and names |
| `src/filters` (splitters, renderers, muxers, transforms) | ~40k | DirectShow filters | Not ported. libmpv (FFmpeg) replaces demux/decode/render |
| `src/Subtitles`, `src/SubPic` | ~35k | Mostly GDI/D3D rendering | Not ported. libass inside libmpv renders SRT/ASS/SSA/PGS/VobSub |
| `src/DSUtil`, `src/CmdUI`, `src/DeCSS` | ~16k | Win32/COM helpers | Replaced by Qt, `QSettings`, libdvdcss/libdvdnav via mpv |
| `src/thirdparty` | ~270k | Bundled (ffmpeg, libass, LAV, etc.) | Dropped. Use distro packages |

A line-by-line port is not realistic: the player logic is welded to the DirectShow
filter graph (`IGraphBuilder`, `IMediaControl`, `IBaseFilter`, ...). The practical
approach is to keep the *front end* and swap the *engine* behind an interface.

## Backend choice

Primary: **libmpv** (`libmpv2` / `mpv-libs`, in Debian, Ubuntu, Fedora, Arch, openSUSE).
It covers what the DirectShow graph did for MPC-HC: container/codec support, hardware
decode (VA-API, NVDEC), subtitles, audio/subtitle track selection, chapters, playback
rate, A-B loop, frame step, aspect override, pan/scan, rotate, DVD/Blu-ray, user shaders.

Optional second backend: **GStreamer** behind the same `PlayerBackend` interface, for
distros or setups that prefer it. Not started; the interface is what makes it possible.

## Feature mapping

| MPC-HC feature | Linux equivalent |
|---|---|
| Main window, menus, seek bar, toolbar, status bar | Qt6 Widgets (`QMainWindow`, custom `SeekBar`) |
| `CmdUI` enable/check state | `QAction` enabled/checked, refreshed from backend state |
| Command IDs (`ID_PLAY_PLAYPAUSE`...) | Same names, generated into `src/Commands.h` from `resource.h` |
| Toolbar bitmaps | Reuse `src/mpc-hc/res/buttons*.svg` (sheet layout documented in `PlayerToolBar.cpp`) |
| Registry / ini settings | `QSettings` (ini in `~/.config`) |
| Video renderers (EVR-CP, madVR, VMR) | mpv `vo=gpu-next` / `gpu`, `hwdec=auto-safe` |
| Pixel shaders (HLSL) | mpv GLSL user shaders; HLSL is not auto-translated |
| Filters menu / property pages | Replaced by audio device, hwdec and decoder options |
| Color management, VSync, GPU flush menus | Mapped to mpv options where one exists, hidden otherwise |
| DVD/BD navigation | `dvd://`, `bd://` via libdvdnav/libbluray in mpv |
| Open Device / TV tuner / capture | Out of scope (V4L2 possible later via mpv `av://v4l2`) |
| Subresync, Statistics panel | Later phase (mpv exposes the needed properties) |

## Phases

1. **Skeleton (this commit):** window, menus, seek bar, volume, open file/URL, play/pause/stop,
   rate, aspect, zoom, fullscreen, audio/subtitle track menus, command table, settings.
2. Playlist bar, recent files, A-B repeat, chapters/navigate menu, frame step, screenshot.
3. Toolbar from the existing SVG sheets, dark theme, Options dialog (ported page by page
   from `PPage*.cpp`), shortcut editor, `wmcmds` default accelerators.
4. DVD/BD menus, info/statistics panes, Subresync, optional GStreamer backend.

## Status of the skeleton

Written against Qt 6 and libmpv's client API. It has **not been compiled yet**: the
environment it was written in had no Qt or libmpv development packages. Expect to fix a
few compile errors on first build.

Build dependencies (Debian/Ubuntu names):
`cmake g++ qt6-base-dev libmpv-dev`

Fedora: `cmake gcc-c++ qt6-qtbase-devel mpv-libs-devel`

## Licensing

MPC-HC is GPLv3; this port stays GPLv3. libmpv is GPLv2+ (LGPLv2.1+ when built without GPL
features), compatible. Keep the original `COPYING.txt` and author credits.
