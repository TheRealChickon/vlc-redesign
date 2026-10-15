# Sharing and licensing

This is an unofficial build. It isn't affiliated with or endorsed by VideoLAN or Juxtopposed.

## Software licences

- **VLC source:** every file keeps its original licence, mostly GPL-2.0-or-later and LGPL-2.1-or-later. See `COPYING`, `vlc/COPYING`, `vlc/COPYING.LIB` and the headers in each file.
- **Subtitle-download adapter:** `vlc/modules/gui/qt/maininterface/redesign_subtitles.hpp` is GPL-3.0-or-later. Its licence text is `licenses/OpenSubtitles-GPL-3.0.txt`.
- **Qt interface plugin:** because it includes that adapter, the built `libqt_plugin.dll` is distributed under GPL-3.0-or-later. The Qt module has no GPL-2.0-only code that would conflict with this.
- **Inter typeface:** Rasmus Andersson and contributors, under the SIL Open Font License (`vlc/modules/gui/qt/pixmaps/redesign/Inter-OFL.txt`).

## Corresponding source

Every binary release publishes these files alongside it:

- **Modified VLC source:** this repository at the release commit, including the build scripts and the contrib recipes and patches in `vlc/contrib/src/`.
- **Third-party library sources:** the exact upstream tarballs for the libraries compiled into the build (Qt, FFmpeg, codecs and others). Each one is verified against the SHA-512 sums pinned in the contrib recipes, and `docs/CONTRIB-SOURCES.tsv` lists their versions.
- **Build instructions:** see `docs/BUILD.md`.

You may redistribute the build under the same licences. Anyone who redistributes binaries must provide the corresponding source in the same way.

## Design

The interface implements the *VLC Redesign* by Juxtopposed: [video](https://www.youtube.com/watch?v=ERFeOgqk324) · [Figma Community file](https://www.figma.com/community/file/1691465999134370980/vlc-redesign). The adapted vector exports are credited in `ATTRIBUTION.md`. Community licences don't cover independently owned third-party material such as posters, clips or trademarks.

## Trademarks

VLC, VideoLAN and the cone logo are trademarks of VideoLAN. Under [VideoLAN's legal page](https://www.videolan.org/legal.html), the VLC mark may be used for GPL-compliant redistributions. This project uses the marks only to identify the software it modifies. It doesn't use the cone as its own logo.

This document summarises licence obligations. It isn't legal advice for every jurisdiction. VideoLAN discusses codec patents and similar topics separately.
