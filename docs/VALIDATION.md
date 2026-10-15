# Completed VLC redesign review build

All 18 findings from the video audit have an implementation. The complete settings registry remains accessible across the 13 categories, and Ctrl+B saves an immediate bookmark without a naming dialog. GitHub stays private.

| # | Video audit item | Completed change |
| --- | --- | --- |
| 1 | [Missing settings caused by stale option bindings](https://www.youtube.com/watch?v=ERFeOgqk324&t=983s) | Current VLC bindings for resume, notifications and geometry, plus notification duration. |
| 2 | [Toolbar bookmark button can be disabled for a newly opened file](https://www.youtube.com/watch?v=ERFeOgqk324&t=900s) | Bookmark button works for an unindexed file at a valid timestamp. |
| 3 | [Settings organization is only partly reworked](https://www.youtube.com/watch?v=ERFeOgqk324&t=983s) | All native preferences regrouped across the 13 categories; Advanced Options and search retain every native key. |
| 4 | [Friendly setting controls are missing](https://www.youtube.com/watch?v=ERFeOgqk324&t=1024s) | Percentage/time/speed sliders, relative or pixel font size, meaningful buffers and audio effect switches. |
| 5 | [Subtitle filename matching is still a number](https://www.youtube.com/watch?v=ERFeOgqk324&t=1024s) | Five readable filename matching rules. |
| 6 | [Bookmark timestamps omit the displayed milliseconds](https://www.youtube.com/watch?v=ERFeOgqk324&t=908s) | Bookmark display includes HH:MM:SS.mmm; native rename, seek, remove and storage retained. |
| 7 | [Shortcut feedback does not include the pressed key](https://www.youtube.com/watch?v=ERFeOgqk324&t=918s) | Native OSD preserves the action/value and appends the actual shortcut once; key display and duration are configurable. |
| 8 | [Extensions still open the mixed native manager](https://www.youtube.com/watch?v=ERFeOgqk324&t=1105s) | Installed add-ons, catalog search, install/remove and package import are in Settings; original native tools remain in Advanced. |
| 9 | [Separate redesigned stream and media dialogs are missing](https://www.youtube.com/watch?v=ERFeOgqk324&t=748s) | Separate file, network, disc, capture, convert and stream entry panels; multiline URLs and optional named native playlists; original advanced wizard remains. |
| 10 | [Current-folder browsing and queue cannot stay open together](https://www.youtube.com/watch?v=ERFeOgqk324&t=680s) | Current-folder browsing, breadcrumbs, media selection/drag and queue insertion coexist with the right native queue. |
| 11 | [Subtitle timeline is a list rather than draggable seek-bar cues](https://www.youtube.com/watch?v=ERFeOgqk324&t=414s) | Real text cues appear above the seek bar; click seeks and horizontal drag adjusts the native delay, with cancel support. |
| 12 | [Saved live presets do not capture every displayed setting](https://www.youtube.com/watch?v=ERFeOgqk324&t=466s) | Live presets include all displayed options, secondary delay, FPS, native framing/deinterlace and audio device/track choices. |
| 13 | [Friendly filter presets and visual previews are absent](https://www.youtube.com/watch?v=ERFeOgqk324&t=524s) | Named equalizer and picture cards with visual icons apply actual VLC filters immediately. |
| 14 | [Quick audio-output selection is absent from the new Audio tab](https://www.youtube.com/watch?v=ERFeOgqk324&t=273s) | Quick Audio device choices use the native enumerator and selector; absent saved devices retain the current output. |
| 15 | [Playback display switches are incomplete](https://www.youtube.com/watch?v=ERFeOgqk324&t=547s) | Waveform, metadata, thumbnail and cue visibility are working native settings. |
| 16 | [Controls use a timer rather than the proposed edge-hover behavior](https://www.youtube.com/watch?v=ERFeOgqk324&t=225s) | Edge hover shows controls; leaving hides them. Locks, keyboard access, fullscreen policy and pinned controls remain. |
| 17 | [Reverse playback needs codec-specific verification](https://www.youtube.com/watch?v=ERFeOgqk324&t=551s) | Native reverse remains where supported. Seekable files otherwise have a discrete reverse video preview with audio paused; tested time decreases and new frames decode. |
| 18 | [Picture-in-picture currently means the embedded library mini-player](https://www.youtube.com/watch?v=ERFeOgqk324&t=234s) | Floating mode uses the existing native window, stays on top and restores geometry/modes. The separate embedded library mini-player remains available. |

Native QA uses an isolated desktop and synthetic media. No mouse, keyboard, capture, focus, launch or restart targeted the user’s running VLC or desktop. Screenshots are Qt pixels; the separately rendered video surface can appear black in them. Every final case exits normally with zero captured QML warnings.

| Case | Boolean observations (including screenshots) | Exit |
| --- | ---: | ---: |
| v99-settings-profiles-final-fixed | 72 | 0 |
| v100-full-video-final-fixed | 66 | 0 |
| v101-toolbar-final-fixed | 87 | 0 |
| v102-feature-parity-final-fixed | 49 | 0 |
| v103-layout-final-fixed | 41 | 0 |

The separate engine check decodes distinct video frames and audio samples and verifies clock, pause, seek, speed and volume. The binary audit retains all 409 VLC plugins across 415 native modules with no unresolved bundled VLC imports. Main VLC, preparser and cache helper explicitly request asInvoker; the waveform helper has no elevation request.

Reverse fallback is a discrete video preview with audio paused, and codec-specific native reverse varies. Popout uses the existing native window. Hardware devices, every codec/protocol, physical discs/casting and a real subtitle account have not all been certified. All original native menus and advanced workflows remain available.

Each release ships the build together with its corresponding source and third-party library sources; see [Sharing and licensing](SHARING.md).

The full review evidence is in docs/evidence. Screenshot booleans are observations, not independent feature tests. Earlier baseline results are historical; these named cases test the final compiled build.
