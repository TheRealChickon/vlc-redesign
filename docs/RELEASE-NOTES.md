# VLC × Juxtopposed redesign: release notes

**Unofficial build. Not affiliated with or endorsed by VideoLAN or Juxtopposed.**

This is a Windows x64 build of VLC 4.0.0-dev (upstream revision `392bc1d0abdb7396b6c97fef206efa1d23868e19`) with a native implementation of the VLC redesign by **Juxtopposed**. The VLC playback engine, native menus and all 409 bundled plugins are unchanged in function.

## Getting started

1. Extract the whole ZIP.
2. Open `VLC-Redesign/vlc.exe`. You don't need an installer or administrator rights.
3. Settings and the library stay in `VLC-Redesign/portable/`. That folder starts empty. Your installed VLC isn't touched.

Check the download against the `.sha256` file published next to it.

## What's in the redesign

- **Library:** Home, Video, Music, Network and Playlists views, with grid and list layouts, search, sorting, pins, favorites and watched state.
- **Player:** redesigned controls, seek thumbnails, a responsive timeline with an audio waveform, a fullscreen title and a mini-player.
- **Sidebar:** the queue, a videos and folder browser, bookmarks and chapters all sit beside playback. **Ctrl+B** saves a bookmark immediately, with millisecond timestamps.
- **Live playback panel:** tabs for playback, video, audio and subtitles. It has full presets, output devices, the actual subtitle cues with drag-to-adjust timing, and friendly effect presets.
- **Settings:** 13 redesigned categories that keep every installed native option. Each category has inline Advanced Options, searchable descriptions and units.
- **Keyboard shortcuts:** a separate editor that captures keys natively and handles conflicts.
- **Customize Player:** control profiles and a customizable toolbar for the video, audio and mini players.
- **Subtitles:** native subtitle controls, local matching and review, a subtitle timeline and an optional OpenSubtitles workflow.
- **Original tools:** the disc, capture, conversion, streaming, effects and advanced native dialogs are still available. They use the redesign's colours in both themes.

## Fixes in this release (click-through bug hunt)

- **Settings:**
  - Float options show their real value, so Volume step reads `12.8` instead of `12.800000190734863`.
  - A default audio or subtitle track of `-1` shows as **Auto**, and clearing the field stores Auto again.
  - The Audio effects checkboxes no longer overlap the Two-pass row. Each settings row now grows to fit its editor.
- **Native Effects dialog:**
  - The equalizer preset dropdown shows the active preset (for example "Flat") instead of a blank entry. Its first entry is labelled "Custom".
  - All seven audio-effect tabs fit without broken scroll buttons. Tab padding is tighter and the scroll arrows are styled.
- **Subtitles:** the subtitle importer opens over the player.
- **Customize Player:** icons are sized evenly.
- **Other areas:** the toolbar editor button list, the onboarding and media dialogs, the library and playback panels, the volume and play controls, and the help dialog received fixes. All of them were checked in the dark and light themes.

## Design consistency pass

Every surface now uses the same control set as the redesigned Settings and the Figma file:

- **Playback panel:**
  - Dropdowns are filled, with the design's chevron and a rounded list that ticks the current item.
  - Fields drop their outlines, and switches use the knob style from Settings.
  - Sliders have a solid handle, and track rows show an accent check.
  - Each tab has an icon.
- **Placeholders:** artwork placeholders and empty-library tiles are dark, translucent tiles instead of a bright grey card. They work in both themes.
- **Other dropdowns:** the Customize Player profile picker and the native dialogs' dropdowns have the same filled style.
- **Fields:** the media dialog and library search fields use the subtle field edge from Settings.

All 14 automated checks passed for this build: 7 scenarios × the dark and light themes. Each one ran on an isolated desktop with synthetic media. Every check passed, with no QML warnings and a clean exit every time.

## Known limits

- **Reverse playback:** native reverse is used when the codec supports it. Otherwise the build falls back to a stepped, seek-based reverse preview. That preview is video only, with the audio paused, and it only works on seekable files.
- **Floating video:** picture-in-picture reuses the native VLC video window. It isn't a separate compositor surface.
- **Untested hardware and formats:** hardware capture devices, discs, casting, every codec and protocol, and real-account subtitle downloads haven't all been tested. Your codec behavior may vary.
- **Pixel accuracy:** this is a development build of VLC 4.0.0-dev. Exact Figma pixels can't be guaranteed on every display or scaling factor.
- **Firewall prompts:** Windows Firewall may ask about network discovery the first time you run it. Local playback doesn't need network access.

## Source code

The complete corresponding source for this build is published with it as `VLC-Redesign-source-<version>.zip` and in the repository's `vlc/` tree. Build instructions are in `docs/BUILD.md`. The third-party libraries were compiled from VideoLAN's pinned contrib recipes, which ship in `vlc/contrib/src/`. `docs/CONTRIB-SOURCES.tsv` lists every package version and its upstream source.

## Credits

- **Design:** *VLC Redesign* by **Juxtopposed**. See the [video](https://www.youtube.com/watch?v=ERFeOgqk324) and the [Figma Community file](https://www.figma.com/community/file/1691465999134370980/vlc-redesign). Her vector exports were adapted for the native interface.
- **VLC:** **VideoLAN** and the VLC contributors: [videolan.org](https://www.videolan.org/vlc/) · [source](https://code.videolan.org/videolan/vlc). VLC, VideoLAN and the cone logo are trademarks of VideoLAN. They're used here only to identify this GPL-compliant redistribution.
- **Inter typeface:** Rasmus Andersson and contributors, under the SIL Open Font License.

## Licence

VLC code keeps its original per-file licences, mostly GPL-2.0-or-later and LGPL-2.1-or-later. See `COPYING.txt` (GPL-2.0), `COPYING.LIB.txt` (LGPL-2.1), `licenses/` and `ATTRIBUTION.md`.

The subtitle-download adapter (`redesign_subtitles.hpp`) is GPL-3.0-or-later. Because of that, the Qt interface plugin `plugins/gui/libqt_plugin.dll` is distributed under GPL-3.0-or-later; its full licence text is in `licenses/`. The design assets are credited to Juxtopposed. See `ATTRIBUTION.md` for their licence status.
