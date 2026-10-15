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

## Polish pass

A follow-up pass tidies the redesigned panels without changing behaviour. It was compiled with `scripts/rebuild-full-video.sh` (no qmlcachegen warnings) and checked with fresh native QA cases in dark and light themes.

- Light theme: the playback panel's tabs, fields, buttons, toggles and sliders, the settings toggles, search placeholder and sidebar, and the welcome and subtitle-import borders now use theme-aware colours instead of fixed dark values.
- Settings: the Advanced Options control is a collapsible section header with a chevron, and its actions (Reset all settings, native add-on tools) now appear beneath it instead of above it. Toggles and category buttons show a keyboard focus ring.
- Welcome: Back, Browse and the folder field match the redesign's button and field style instead of the default Qt look; step dots are spaced and stay centred; Next has hover/pressed states.
- Equalizer: each band shows its boost or cut as a fill from 0 dB.
- Playback tab: the waveform and media-information switches now show their labels; they previously rendered as two unlabelled ON/OFF toggles. The toggles also expose an accessible name.
- Welcome step dots switch immediately. A colour animation added earlier in this pass left the previous dot lit in captures.
- QA: `VLC_REDESIGN_QA_SCHEME` (0 system, 1 light, 2 dark; default dark) lets native QA capture the light theme. `scripts/run_polish_pr1_regressions.py` runs the five regression modes in dark and four in light.

| Case (final build) | Boolean observations | Failures | QML warnings | Exit |
| --- | ---: | ---: | ---: | ---: |
| p3r-settings-profiles-dark | 72 | 0 | 0 | 0 |
| p3r-full-video-dark | 66 | 0 | 0 | 0 |
| p3-toolbar-dark | 87 | 0 | 0 | 0 |
| p3-feature-parity-dark | 49 | 0 | 0 | 0 |
| p3-layout-dark | 41 | 0 | 0 | 0 |
| p3-settings-profiles-light | 72 | 0 | 0 | 0 |
| p3-full-video-light | 66 | 0 | 0 | 0 |
| p3-feature-parity-light | 49 | 0 | 0 | 0 |
| p3-layout-light | 41 | 0 | 0 | 0 |

The first p3 runs of settings-profiles-dark and full-video-dark each failed timing-sensitive checks under heavy unrelated CPU load: the subtitle importer was looked up after the 1:00 fixture finished at 1.25× speed, and the reverse preview did not start. Both passed unchanged on rerun (p3r), and the same checks passed in the light runs of the same build. These checks remain timing-sensitive.

### Second round

- Volume slider: the Figma track is recoloured for the light theme; it previously stayed dark. The toolbar editor's preview, which has no palette, keeps the dark track.
- Welcome: the dialog stays above non-modal panels opened after it. The folder row matches Figma: a 36 px field with a 2 px border and a 114 px Browse button in the design's colours.

| Case (p5 build) | Boolean observations | Failures | QML warnings | Exit |
| --- | ---: | ---: | ---: | ---: |
| p5-settings-profiles-dark | 72 | 0 | 0 | 0 |
| p5-full-video-dark | 66 | 0 | 0 | 0 |
| p5-toolbar-dark | 87 | 0 | 0 | 0 |
| p5-feature-parity-dark | 49 | 0 | 0 | 0 |
| p5-layout-dark | 41 | 0 | 0 | 0 |
| p5-settings-profiles-light | 72 | 0 | 0 | 0 |
| p5-full-video-light | 66 | 0 | 0 | 0 |
| p5-feature-parity-light | 49 | 0 | 0 | 0 |
| p5-layout-light | 41 | 0 | 0 | 0 |

The intermediate p4 build printed two QML warnings in the toolbar case (the preview palette was null); p5 fixed them.

### Third round (Figma parity)

- Settings dropdowns follow the design reference: a 24 px chevron (flipped while open), a rounded list, and the current choice highlighted with an accent check. They replace the default spinner arrows and list. The p6 build made each dropdown as tall as the chevron's SVG render. QA booleans missed it and the screenshots showed it; p7 pins the height.
- Home: "Frequently Visited Folders" uses square folder tiles sized like the music covers, with the name beneath, as in the Figma home frame.
- QA: the parity case also captures an open settings dropdown (`settings-dropdown.png`, `settingsDropdownOpens`).

| Case (p7 build) | Boolean observations | Failures | QML warnings | Exit |
| --- | ---: | ---: | ---: | ---: |
| p7-settings-profiles-dark | 72 | 0 | 0 | 0 |
| p7-full-video-dark | 66 | 0 | 0 | 0 |
| p7-toolbar-dark | 87 | 0 | 0 | 0 |
| p7-feature-parity-dark | 51 | 0 | 0 | 0 |
| p7-layout-dark | 41 | 0 | 0 | 0 |
| p7-settings-profiles-light | 72 | 0 | 0 | 0 |
| p7-full-video-light | 66 | 0 | 0 | 0 |
| p7-feature-parity-light | 51 | 0 | 0 | 0 |
| p7-layout-light | 41 | 0 | 0 | 0 |

### Fourth round (p8 build)

- Library side panel: removed the bright dividers between its tabs (the default TabBar background showed through).
- Home folder tiles use the design's BG/A3 colour (`#1b1b1b` dark, `#eaeaea` light) so they stand out from the page, especially in light theme.

The first p8 run was stopped at the owner's request. With the owner's approval, the p8b run then passed:

| Case (p8 build) | Boolean observations | Failures | QML warnings | Exit |
| --- | ---: | ---: | ---: | ---: |
| p8b-settings-profiles-dark | 72 | 0 | 0 | 0 |
| p8b-full-video-dark | 66 | 0 | 0 | 0 |
| p8b-toolbar-dark | 87 | 0 | 0 | 0 |
| p8b-feature-parity-dark | 51 | 0 | 0 | 0 |
| p8b-layout-dark | 41 | 0 | 0 | 0 |
| p8b-settings-profiles-light | 72 | 0 | 0 | 0 |
| p8b-full-video-light | 66 | 0 | 0 | 0 |
| p8b-feature-parity-light | 51 | 0 | 0 | 0 |
| p8b-layout-light | 41 | 0 | 0 | 0 |

Known differences kept: the player's cover art is square in this build but rounded in Figma. Rounding it safely needs the live blurred backdrop to keep its mipmapped texture, which needs a separate verified change.
