// SPDX-License-Identifier: GPL-2.0-or-later
// Native preferences, using the VLC Redesign by Juxtopposed as the visual reference.
import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Dialogs
import VLC.Style
import VLC.MediaLibrary

C.Popup {
    id: root
    objectName: "redesignSettings"
    modal: true
    focus: true
    closePolicy: C.Popup.CloseOnEscape
    padding: 0
    width: Math.min(1048 * VLCStyle.scale, parent.width - 24)
    height: Math.min(865 * VLCStyle.scale, parent.height - 24)
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    property int category: 0
    property bool advanced: false
    property bool shortcutsOnly: false
    property bool requestedShortcuts: false
    property var infoOption: ({})
    property var options: []
    property var draft: ({})
    property int draftRevision: 0
    property int themeIndex: 2
    property string failure: ""
    property var pendingShortcut: ({})
    property alias searchQuery: search.text
    property string requestedSearch: ""
    function openAtOption(key) { requestedSearch = key; root.open() }
    function openShortcuts() { requestedShortcuts = true; root.open() }
    readonly property real unit: VLCStyle.scale
    readonly property bool light: MainCtx.colorScheme.scheme === ColorSchemeModel.Day
    readonly property color paper: light ? "#f8f8f8" : "#0e0e0e"
    readonly property color panel: light ? "#eaeaea" : "#191919"
    readonly property color field: light ? "#ffffff" : "#090909"
    readonly property color textColor: light ? "#333333" : "#8f8f8f"
    readonly property color titleColor: light ? "#242424" : "#c6c6c6"
    readonly property color mutedColor: light ? "#777777" : "#6b6b6b"
    readonly property color hoverColor: light ? "#dddddd" : "#222222"
    readonly property color knobColor: light ? "#bdbdbd" : "#404040"
    readonly property color accent: theme.accent
    readonly property var categories: [
        {title:qsTr("Global Settings"),name:qsTr("General"),icon:"7",group:qsTr("App"),subs:[101,102,602]},
        {title:qsTr("Interface Settings"),name:qsTr("Interface"),icon:"3",subs:[102]},
        {title:qsTr("Privacy Settings"),name:qsTr("Privacy"),icon:"8",subs:[101,701]},
        {title:qsTr("Mouse, Keyboard & Remote Controls"),name:qsTr("Controls"),icon:"9",subs:[103,104]},
        {title:qsTr("Playback Preferences"),name:qsTr("Playback"),icon:"10",group:qsTr("Media"),subs:[401,403]},
        {title:qsTr("Video Defaults"),name:qsTr("Video"),icon:"11",subs:[301,302,303,306]},
        {title:qsTr("Audio Defaults"),name:qsTr("Audio"),icon:"12",subs:[201,202,203,204,206]},
        {title:qsTr("Subtitle Defaults"),name:qsTr("Subtitles"),icon:"13",subs:[305,406]},
        {title:qsTr("Playlists, Folders & Files"),name:qsTr("Library"),icon:"14",subs:[701,702,703]},
        {title:qsTr("Network Connections & Security"),name:qsTr("Network"),icon:"15",group:qsTr("System"),subs:[603,402,407]},
        {title:qsTr("Performance Settings"),name:qsTr("Performance"),icon:"16",subs:[602,404,405]},
        {title:qsTr("Recording, Casting & Streaming Out"),name:qsTr("Recording"),icon:"17",subs:[501,502,503,504,505,507,508]},
        {title:qsTr("Add-ons & Scripts"),name:qsTr("Extensions"),icon:"18",subs:[103,702]}]

    palette.window:paper
    palette.base:field
    palette.button:panel
    palette.buttonText:titleColor
    palette.text:textColor
    palette.highlight:accent
    palette.highlightedText:paper
    palette.light:panel
    palette.mid:panel

    ColorContext { id: theme; palette: VLCStyle.palette; colorSet: ColorContext.Window }

    component SecondaryButton: C.Button {
        height: 34 * root.unit
        leftPadding: 16 * root.unit
        rightPadding: 16 * root.unit
        font.pixelSize: 14 * root.unit
        background: Rectangle {
            color: parent.hovered ? root.hoverColor : root.panel
            radius: 6 * root.unit
            border.color: parent.activeFocus ? root.accent : "transparent"
            border.width: 1
        }
        contentItem: Text {
            text: parent.text
            color: root.textColor
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    function value(opt) {
        const revision = draftRevision
        return Object.prototype.hasOwnProperty.call(draft, opt.key) ? draft[opt.key] : opt.value
    }
    function edit(opt, v) {
        const next = Object.assign({}, draft)
        if (v === opt.value) delete next[opt.key]
        else next[opt.key] = v
        draft = next
        draftRevision++
        failure = ""
    }
    function find(key) { return options.find(o => o.key === key) }
    function decorated(opt, label) {
        const o = Object.assign({}, opt)
        o.label = label || o.label
        if (o.choices) o.choices = o.choices.map(c => Object.assign({}, c))
        const percent = {"freetype-shadow-distance":.01,"freetype-opacity":2.55,"freetype-background-opacity":2.55,"qt-opacity":.01,"qt-fs-opacity":.01}
        const seconds = {"mouse-hide-timeout":1000,"qt-redesign-notification-duration":1000,"qt-redesign-feedback-duration":1000,"video-title-timeout":1000,"audio-desync":1000,"sub-delay":10}
        if (o.key in percent) Object.assign(o,{editor:"range",factor:percent[o.key],from:0,to:100,step:1,suffix:" %"})
        if (o.key in seconds) Object.assign(o,{editor:"range",factor:seconds[o.key],from:o.key==="audio-desync"||o.key==="sub-delay"?-10:0,to:10,step:.1,suffix:" s"})
        if (["brightness","contrast","saturation","gamma"].includes(o.key)) Object.assign(o,{editor:"range",factor:1,from:0,to:o.key==="gamma"?10:2,step:.01,suffix:""})
        if (o.key==="hue") Object.assign(o,{editor:"range",factor:1,from:0,to:360,step:1,suffix:"°"})
        if (o.key==="qt-max-volume") Object.assign(o,{editor:"range",factor:1,from:60,to:300,step:1,suffix:" %"})
        if (o.key==="rate") Object.assign(o,{editor:"range",factor:1,from:.25,to:4,step:.05,suffix:"×"})
        if (o.key==="freetype-fontsize") o.editor="fontSize"
        if (o.key==="audio-filter") o.editor="audioFilters"
        if (["file-caching","network-caching","disc-caching","live-caching"].includes(o.key)) {
            const v=Number(value(o))
            o.choices=[{value:0,label:qsTr("No buffer")},{value:300,label:qsTr("Low latency · 0.3 s")},{value:1000,label:qsTr("Standard · 1 s")},{value:3000,label:qsTr("Stable playback · 3 s")}]
            if(!o.choices.some(c=>c.value===v))o.choices.push({value:v,label:qsTr("Custom · %1 s").arg((v/1000).toFixed(3))})
        }
        if (o.key==="sub-autodetect-fuzzy") o.choices=[{value:0,label:qsTr("Do not detect subtitles")},{value:1,label:qsTr("Any subtitle file")},{value:2,label:qsTr("File name contains the title")},{value:3,label:qsTr("Title with extra characters")},{value:4,label:qsTr("Exactly the title")}]
        if (o.key==="qt-redesign-auto-hide") o.choices=[{value:0,label:qsTr("Never")},{value:1,label:qsTr("In fullscreen only")},{value:2,label:qsTr("While playing video")}]
        return o
    }
    function presentationCategory(opt) {
        for (let i=0;i<presets.length;i++) for (const s of presets[i]) if(s.rows.some(r=>r[0]===opt.key))return i
        if(opt.hotkey)return 3
        if(/^(sub-|subs|freetype-|spu-|ass-|osd|video-title)/.test(opt.key))return 7
        if(/^(sout-|input-record|record-)/.test(opt.key))return 11
        if(/^(http-|https-|rtsp-|rtp-|udp-|tcp-|srt-|tls-|gnutls-|ftp-|network-|live-caching)/.test(opt.key))return 9
        if(/^(avcodec-|hw-|dec-|clock-|file-caching|disc-caching|drop-|skip-)/.test(opt.key))return 10
        if(/^(audio-|volume|equalizer-|compressor-|spatializer-|stereowiden-|norm-|replay-gain|aout)/.test(opt.key))return 6
        if(/^(video-|snapshot-|crop|aspect|zoom|deinterlace|brightness|contrast|saturation|gamma|hue|transform-|sharpen-|grain-)/.test(opt.key))return 5
        if(/^(playlist-|media-library|recursive|show-hidden|save-recent)/.test(opt.key))return 8
        if(/^(lua-|addons-|extension)/.test(opt.key))return 12
        for(let i=1;i<categories.length;i++)if(categories[i].subs.includes(opt.subcategory))return i
        return 0
    }
    function presentationSection(opt) {
        if(opt.hotkey)return qsTr("Keyboard shortcuts")
        if(/filter|equalizer|compressor|spatial|sharpen|transform|grain/.test(opt.key))return qsTr("Effects and filters")
        if(/language|track|encoding|codec/.test(opt.key))return qsTr("Tracks, languages and formats")
        if(/cache|buffer|clock|thread|priority/.test(opt.key))return qsTr("Processing and buffering")
        if(/path|file|folder|directory|mrl/.test(opt.key))return qsTr("Files and locations")
        return qsTr("Additional preferences")
    }
    function resetAll(){options.forEach(o=>edit(o,o.defaultValue));themeIndex=2}
    function resetShortcuts() { options.filter(o=>o.hotkey).forEach(o=>edit(o,o.defaultValue)) }
    function resetPicture() { ["hue","brightness","contrast","saturation","gamma"].forEach(k=>{const o=find(k);if(o)edit(o,o.defaultValue)}) }
    function presentedRows() {
        const result=[]
        for(const category of presets)for(const section of category)for(const row of section.rows)result.push(row)
        return result
    }
    function rows() {
        const query = search.text.trim().toLowerCase()
        const searchable = shortcutsOnly ? options.filter(o => o.hotkey) : options
        if (query.length) return searchable.map(o=>decorated(o, presentedRows().find(r=>r[0]===o.key)?.[1])).filter(o =>
            (o.label + " " + o.key + " " + o.description).toLowerCase().includes(query))
        const result = []
        if (shortcutsOnly) {
            result.push({type:"heading",label:qsTr("Keyboard shortcuts")})
            result.push(...options.filter(o => o.hotkey && !o.key.startsWith("global-")))
            result.push({type:"heading",label:qsTr("Global shortcuts")})
            result.push(...options.filter(o => o.hotkey && o.key.startsWith("global-")))
            return result
        }
        const groups = presets[category]
        for (const section of groups) {
            const entries = section.rows.map(r => {
                const option = find(r[0])
                return option ? decorated(option,r[1]) : null
            }).filter(o => o !== null)
            if (entries.length) {
                result.push({type:"heading",label:section.title})
                result.push(...entries)
            }
        }
        if (advanced) {
            const basicKeys = new Set(presentedRows().map(r=>r[0]))
            const extra = options.filter(o => presentationCategory(o)===category && !basicKeys.has(o.key) && !o.hotkey)
            result.push({type:"heading",label:qsTr("Advanced Options")})
            const sections={}
            for(const option of extra) {const section=presentationSection(option);if(!sections[section])sections[section]=[];sections[section].push(decorated(option))}
            for(const section of Object.keys(sections).sort()) {
                result.push({type:"heading",label:section})
                result.push(...sections[section].sort((a,b)=>a.label.localeCompare(b.label)))
            }
        }
        return result
    }
    function assignShortcut(opt, shortcut) {
        const conflicts = options.filter(candidate => candidate.hotkey && candidate.key !== opt.key
            && candidate.key.startsWith("global-") === opt.key.startsWith("global-")
            && String(value(candidate)).split("\t").includes(shortcut))
        if (conflicts.length) {
            pendingShortcut = {option:opt, shortcut:shortcut, conflicts:conflicts}
            shortcutConflict.open()
        } else edit(opt, shortcut)
    }
    function save() {
        if (!MainCtx.saveRedesignConfig(draft)) {
            failure = qsTr("Could not save these settings. Check the entered values and configuration file permissions.")
            return
        }
        MainCtx.colorScheme.selectedIndex = themeIndex
        VLCStyle.palette.refreshAccent()
        VLCStyle.darkPalette.refreshAccent()
        root.close()
    }
    onOpened: {
        shortcutsOnly = requestedShortcuts
        requestedShortcuts = false
        options = MainCtx.redesignConfigOptions()
        draft = ({})
        draftRevision++
        themeIndex = MainCtx.colorScheme.selectedIndex
        failure = ""
        search.text = requestedSearch
        requestedSearch = ""
        advanced = false
    }
    onClosed: { shortcutConflict.close(); optionInfo.close(); options = []; draft = ({}) }

    // Each displayed control is backed by a registered option from the installed
    // modules. Additional module settings remain available under Advanced Options.
    readonly property var presets: [
        [{title:qsTr("Appearance"),rows:[]},
         {title:qsTr("Startup"),rows:[["qt-error-dialogs",qsTr("Show confirmation and error dialogs")],["qt-updates-notif",qsTr("Check for updates automatically")]]},
         {title:qsTr("Diagnostics"),rows:[["verbose",qsTr("Developer log detail")],["file-logging",qsTr("Save a log")],["logfile",qsTr("Log file")],["log-verbose",qsTr("Log details")]]}],
        [{title:qsTr("Window"),rows:[["embedded-video",qsTr("Play video inside main window")],["qt-video-autoresize",qsTr("Resize window to fit video")],["qt-redesign-remember-geometry",qsTr("Remember window size and position")],["video-on-top",qsTr("Keep window above other windows")],["video-deco",qsTr("Show window decorations")],["qt-name-in-title",qsTr("Show the file name in the title bar")],["qt-fullscreen-screennumber",qsTr("Open fullscreen on")],["qt-opacity",qsTr("Window opacity")],["qt-fs-opacity",qsTr("Fullscreen controls opacity")]]},
         {title:qsTr("Playback Controls"),rows:[["qt-minimal-view",qsTr("Start in compact mode")],["qt-redesign-auto-hide",qsTr("Auto-hide the controls")],["mouse-hide-timeout",qsTr("Hide after")],["qt-fs-controller",qsTr("Show controls in fullscreen")]]},
         {title:qsTr("Notifications and System Tray"),rows:[["qt-system-tray",qsTr("Show a system tray icon")],["qt-start-minimized",qsTr("Start minimized to the tray")],["qt-auto-raise",qsTr("Bring VLC to the front when playback starts")],["qt-pause-minimized",qsTr("Pause video when window is minimized")],["qt-notification",qsTr("Show a popup when the track changes")],["qt-redesign-notification-duration",qsTr("Popup duration")]]},
         {title:qsTr("Playlist"),rows:[["qt-playlist-tree",qsTr("Show the playlist as a tree")],["qt-display-mode",qsTr("Show cover art in the window when playing audio")]]}],
        [{title:qsTr("History"),rows:[["save-recentplay",qsTr("Save recently played items")],["qt-redesign-history-filter",qsTr("Ignore file names containing (separate words with |)")]]},
         {title:qsTr("Network"),rows:[["metadata-network-access",qsTr("Fetch titles and cover art from the internet")],["lastfm-username",qsTr("Last.fm username")],["lastfm-password",qsTr("Last.fm password")]]}],
        [{title:qsTr("Skip Amounts"),rows:[["extrashort-jump-size",qsTr("Very short skip (s)")],["short-jump-size",qsTr("Short skip (s)")],["medium-jump-size",qsTr("Medium skip (s)")],["long-jump-size",qsTr("Long skip (s)")]]},
         {title:qsTr("Mouse"),rows:[["mouse-events",qsTr("Enable mouse controls")],["hotkeys-mousewheel-mode",qsTr("Scroll wheel")],["hotkeys-x-wheel-mode",qsTr("Horizontal scroll")]]}],
        [{title:qsTr("Startup"),rows:[["restore-playback-pos",qsTr("Continue where you left off")],["playlist-autostart",qsTr("Start playing as soon as a file opens")],["rate",qsTr("Default playback speed")]]},
         {title:qsTr("Tracks"),rows:[["preferred-resolution",qsTr("Preferred video quality")],["menu-language",qsTr("Preferred menu language")],["audio-track",qsTr("Default audio track")],["sub-track",qsTr("Default subtitle track")]]},
         {title:qsTr("Order"),rows:[["loop",qsTr("Repeat all")],["repeat",qsTr("Repeat one")],["random",qsTr("Shuffle")],["play-and-pause",qsTr("Stop after each item")],["play-and-exit",qsTr("Exit when playback finishes")]]},
         {title:qsTr("Seeking"),rows:[["input-fast-seek",qsTr("Fast seeking")],["qt-redesign-preview",qsTr("Show thumbnail while scrubbing")]]},
         {title:qsTr("Files & Discs"),rows:[["dvd",qsTr("Default optical drive")],["avi-index",qsTr("Repair damaged AVI files")]]}],
        [{title:qsTr("Output"),rows:[["video",qsTr("Enable video")],["fullscreen",qsTr("Start videos in fullscreen")],["disable-screensaver",qsTr("Stop the screensaver during playback")],["video-wallpaper",qsTr("Play video as desktop wallpaper")]]},
         {title:qsTr("Picture"),rows:[["hue",qsTr("Hue")],["brightness",qsTr("Brightness")],["contrast",qsTr("Contrast")],["saturation",qsTr("Saturation")],["gamma",qsTr("Gamma")]]},
         {title:qsTr("Deinterlacing"),rows:[["deinterlace",qsTr("Deinterlacing")],["deinterlace-mode",qsTr("Method")]]},
         {title:qsTr("Framing"),rows:[["aspect-ratio",qsTr("Aspect ratio")],["crop",qsTr("Crop")],["autoscale",qsTr("Scale video to the window")],["zoom",qsTr("Zoom")],["align",qsTr("Position in the window")]]},
         {title:qsTr("Snapshots"),rows:[["snapshot-path",qsTr("Save to")],["snapshot-prefix",qsTr("File name prefix")],["snapshot-format",qsTr("Format")],["snapshot-width",qsTr("Width")],["snapshot-height",qsTr("Height")],["snapshot-sequential",qsTr("Number files sequentially")],["snapshot-preview",qsTr("Show a preview after capture")]]}],
        [{title:qsTr("Output"),rows:[["audio",qsTr("Enable audio")],["aout",qsTr("Output device")],["audio-channels",qsTr("Speaker setup")]]},
         {title:qsTr("Volume"),rows:[["qt-max-volume",qsTr("Maximum volume")],["volume",qsTr("Startup volume")],["volume-step",qsTr("Volume step")]]},
         {title:qsTr("Loudness"),rows:[["norm-max-level",qsTr("Target level")],["audio-replay-gain-mode",qsTr("Replay gain")],["audio-replay-gain-preamp",qsTr("Replay gain preamp")]]},
         {title:qsTr("Effects"),rows:[["equalizer-preset",qsTr("Equalizer preset")],["equalizer-2pass",qsTr("Two-pass")],["audio-filter",qsTr("Audio effects")]]},
         {title:qsTr("Tracks"),rows:[["audio-language",qsTr("Preferred audio language")],["audio-desync",qsTr("Audio delay (ms)")]]},
         {title:qsTr("Visualization"),rows:[["audio-visual",qsTr("Visualizer")]]}],
        [{title:qsTr("Tracks & Languages"),rows:[["spu",qsTr("Enable subtitles")],["sub-language",qsTr("Preferred language")],["subsdec-encoding",qsTr("Text encoding")]]},
         {title:qsTr("Loading Subtitles"),rows:[["sub-autodetect-file",qsTr("Load subtitle files automatically")],["sub-autodetect-fuzzy",qsTr("File name matching")]]},
         {title:qsTr("Appearance"),rows:[["freetype-font",qsTr("Font")],["freetype-fontsize",qsTr("Font size")],["freetype-bold",qsTr("Bold")],["freetype-color",qsTr("Text color")],["freetype-opacity",qsTr("Text opacity")],["freetype-outline-thickness",qsTr("Outline thickness")],["freetype-outline-color",qsTr("Outline color")],["freetype-shadow-color",qsTr("Shadow color")],["freetype-shadow-distance",qsTr("Shadow distance")],["freetype-background-opacity",qsTr("Background opacity")],["freetype-background-color",qsTr("Background color")],["sub-margin",qsTr("Vertical position")]]},
         {title:qsTr("Timing"),rows:[["sub-delay",qsTr("Subtitle delay")]]},
         {title:qsTr("On-Screen Messages"),rows:[["osd",qsTr("Show feedback for shortcuts")],["qt-redesign-key-feedback",qsTr("Show the pressed shortcut key")],["qt-redesign-feedback-duration",qsTr("Show feedback for")],["video-title-show",qsTr("Show filename when a video starts")],["video-title-timeout",qsTr("Show filename for")]]}],
        [{title:qsTr("Opening Files"),rows:[["one-instance",qsTr("Only allow one VLC window")],["one-instance-when-started-from-file",qsTr("Apply to files opened from the file manager")],["playlist-enqueue",qsTr("Add to the playlist instead of replacing it")],["recursive",qsTr("When opening a folder")],["show-hiddenfiles",qsTr("Show hidden files")]]},
         {title:qsTr("Metadata & Artwork"),rows:[["metadata-network-access",qsTr("Fetch titles and artwork")],["media-library",qsTr("Use the media library")]]}],
        [{title:qsTr("Buffering"),rows:[["file-caching",qsTr("Local files (ms)")],["disc-caching",qsTr("Discs (ms)")],["network-caching",qsTr("Network streams (ms)")],["live-caching",qsTr("Live capture (ms)")]]},
         {title:qsTr("Proxy"),rows:[["http-proxy",qsTr("HTTP proxy")],["http-proxy-pwd",qsTr("HTTP proxy password")]]},
         {title:qsTr("Security"),rows:[["gnutls-system-trust",qsTr("Verify certificates against the system trust store")],["gnutls-priorities",qsTr("Cipher preference")]]}],
        [{title:qsTr("Decoding"),rows:[["hw-dec",qsTr("Hardware-accelerated decoding")],["dec-dev",qsTr("Preferred decoder device")],["postproc-q",qsTr("Video post-processing")],["avcodec-skiploopfilter",qsTr("Skip the H.264 loop filter")],["avcodec-threads",qsTr("Decoding threads")]]},
         {title:qsTr("Smooth Playback"),rows:[["drop-late-frames",qsTr("Drop frames when the system can't keep up")],["high-priority",qsTr("Give VLC real-time priority")]]},
         {title:qsTr("Clock"),rows:[["clock-synchro",qsTr("Clock synchronisation")],["clock-jitter",qsTr("Clock jitter (ms)")],["clock-average",qsTr("Clock reference averaging")]]}],
        [{title:qsTr("Recording"),rows:[["input-record-path",qsTr("Save recordings to")],["input-record-native",qsTr("Record without re-encoding when possible")]]},
         {title:qsTr("Casting"),rows:[["sout-chromecast-conversion-quality",qsTr("Cast quality")]]},
         {title:qsTr("Streaming"),rows:[["sout-display",qsTr("Play locally while streaming")],["sout-audio",qsTr("Include audio")],["sout-video",qsTr("Include video")],["sout-spu",qsTr("Include subtitles")],["sout-x264-preset",qsTr("Encoding preset")],["sout-x264-tune",qsTr("Tuning")]]}],
        [{title:qsTr("Add-ons & Scripts"),rows:[["qt-autoload-extensions",qsTr("Load add-ons on startup")],["lua-intf",qsTr("Interface scripts")],["lua-sd",qsTr("Discovery scripts")]]}]]

    background: Rectangle { color: root.paper; radius: 14 * root.unit; border.color: root.panel; border.width: 1 }
    contentItem: Item {
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 50 * root.unit
            color: root.field
            radius: 14 * root.unit
            Rectangle { anchors {left:parent.left;right:parent.right;bottom:parent.bottom} height:14*root.unit;color:root.field }
            C.TextField {
                id: search
                x: 8 * root.unit; y: 8 * root.unit
                width: Math.min(200 * root.unit, root.width * .27); height: 34 * root.unit
                leftPadding: 40 * root.unit
                placeholderText: qsTr("Search")
                color: root.titleColor; placeholderTextColor: root.mutedColor
                font.pixelSize: 14 * root.unit
                background: Rectangle { color: root.panel; radius: 6 * root.unit }
                Image { x:8*root.unit;anchors.verticalCenter:parent.verticalCenter;width:24*root.unit;height:width;source:"qrc:/redesign/settings-imgFrame5.svg" }
            }
            Text {
                x: 240 * root.unit; anchors.verticalCenter:parent.verticalCenter
                width: parent.width - x - 56 * root.unit
                text: root.shortcutsOnly ? qsTr("Keyboard Shortcuts") : search.text ? qsTr("Search Settings") : root.categories[root.category].title
                color: root.textColor; font.pixelSize:16*root.unit; elide:Text.ElideRight
            }
            C.Button {
                anchors {right:parent.right;verticalCenter:parent.verticalCenter;rightMargin:8*root.unit}
                width:40*root.unit;height:34*root.unit
                Accessible.name:qsTr("Close settings")
                background:Rectangle { color:parent.hovered?root.panel:"transparent";radius:6*root.unit }
                contentItem:Image { source:"qrc:/redesign/settings-imgFrame6.svg";sourceSize.width:24*root.unit;fillMode:Image.Pad }
                onClicked:root.close()
            }
        }
        Rectangle {
            x:8*root.unit;y:58*root.unit;width:200*root.unit;height:parent.height-y-8*root.unit
            visible:!root.shortcutsOnly
            color:root.panel;radius:6*root.unit
            C.ScrollView {
                anchors.fill:parent;clip:true
                C.ScrollBar.horizontal.policy:C.ScrollBar.AlwaysOff
                Column {
                    width:200*root.unit;spacing:0
                    Repeater {
                        model:root.categories
                        delegate:Column {
                            required property var modelData
                            required property int index
                            width:200*root.unit
                            Text {
                                visible:!!modelData.group; text:modelData.group||""
                                height:visible?46*root.unit:0;leftPadding:16*root.unit;verticalAlignment:Text.AlignVCenter
                                color:root.mutedColor;font.pixelSize:14*root.unit
                            }
                            C.Button {
                                x:6*root.unit;width:188*root.unit;height:46*root.unit
                                padding:8*root.unit
                                Accessible.name:modelData.name
                                background:Rectangle {color:root.category===index||parent.hovered?root.hoverColor:"transparent";radius:4*root.unit;border.color:parent.visualFocus?root.accent:"transparent"}
                                contentItem:Row {
                                    spacing:12*root.unit
                                    Image { width:24*root.unit;height:width;anchors.verticalCenter:parent.verticalCenter;source:VLCStyle.iconUrl("settings-imgFrame"+modelData.icon+".svg",root.category===index?root.accent:root.textColor) }
                                    Text {text:modelData.name;color:root.category===index?root.titleColor:root.textColor;anchors.verticalCenter:parent.verticalCenter;font.pixelSize:14*root.unit}
                                }
                                onClicked: {root.category=index;root.advanced=false;search.text="";settingList.positionViewAtBeginning()}
                            }
                        }
                    }
                }
            }
        }
        ListView {
            id:settingList
            objectName:"redesignSettingsList"
            x:(root.shortcutsOnly?32:240)*root.unit;y:66*root.unit;width:parent.width-x-24*root.unit;height:parent.height-y-66*root.unit
            clip:true;spacing:2*root.unit
            model:root.rows()
            C.ScrollBar.vertical:C.ScrollBar {policy:C.ScrollBar.AsNeeded}
            header:Column {
                width:settingList.width;spacing:16*root.unit
                visible:root.category===0&&!root.shortcutsOnly&&!search.text
                height:visible?implicitHeight:0
                Text {text:qsTr("Appearance");color:root.titleColor;font.pixelSize:16*root.unit}
                RowLayout {
                    width:parent.width;height:34*root.unit
                    Text {text:qsTr("Interface language");color:root.textColor;font.pixelSize:14*root.unit;Layout.fillWidth:true}
                    Loader {
                        Layout.preferredWidth:300*root.unit
                        property var opt:root.find("qt-redesign-language")
                        active:!!opt
                        sourceComponent:choiceControl
                    }
                }
                RowLayout {
                    width:parent.width;height:34*root.unit
                    Text {text:qsTr("Theme");color:root.textColor;font.pixelSize:14*root.unit;Layout.fillWidth:true}
                    Row {
                        spacing:2*root.unit
                        Repeater {
                            model:[{name:qsTr("Dark"),value:2},{name:qsTr("Light"),value:1},{name:qsTr("System"),value:0}]
                            delegate:C.Button {
                                required property var modelData
                                width:100*root.unit;height:34*root.unit;text:modelData.name
                                background:Rectangle {color:root.themeIndex===modelData.value?root.accent:root.panel;radius:6*root.unit}
                                contentItem:Text {text:parent.text;color:root.themeIndex===modelData.value?VLCStyle.onAccent(root.accent):root.textColor;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;font.pixelSize:14*root.unit}
                                onClicked:root.themeIndex=modelData.value
                            }
                        }
                    }
                }
                RowLayout {
                    width:parent.width;height:34*root.unit
                    Text {text:qsTr("Accent color");color:root.textColor;font.pixelSize:14*root.unit;Layout.fillWidth:true}
                    Row {
                        spacing:6*root.unit
                        Repeater {
                            model:["#d37918","#e15032","#c49e23","#19a234","#18aad3","#185dd3","#8e42ff","#f052b3","#afafaf"]
                            delegate:C.Button {
                                required property string modelData
                                width:28*root.unit;height:28*root.unit
                                Accessible.name:qsTr("Accent color %1").arg(modelData)
                                background:Rectangle {radius:6*root.unit;color:modelData;border.width:root.value({key:"qt-accent-color",value:"default"})===modelData?2:0;border.color:root.titleColor}
                                onClicked:root.edit(root.find("qt-accent-color"),modelData)
                            }
                        }
                    }
                }
            }
            delegate:Item {
                id:row
                required property var modelData
                width:settingList.width
                height:modelData.type==="heading"?56*root.unit:Math.max((modelData.editor==="audioFilters"?172:modelData.editor==="range"?64:40)*root.unit,label.implicitHeight+8*root.unit,optionEditor.implicitHeight+8*root.unit)
                Text {
                    id:label
                    anchors {left:parent.left;verticalCenter:parent.verticalCenter}
                    width:modelData.type==="heading"?parent.width:Math.max(80*root.unit,parent.width-optionEditor.width-40*root.unit)
                    text:modelData.label
                    wrapMode:Text.WordWrap
                    color:modelData.type==="heading"?root.titleColor:root.textColor
                    font.pixelSize:(modelData.type==="heading"?16:14)*root.unit
                }
                C.ToolTip.visible: hover.hovered&&modelData.type!=="heading"&&!!modelData.description
                C.ToolTip.text:modelData.description||""
                C.ToolTip.delay:800
                HoverHandler {id:hover}
                C.ToolButton {
                    objectName:"redesignSettingInfo-" + (row.modelData.key || "heading")
                    anchors {right:optionEditor.left;rightMargin:8*root.unit;verticalCenter:parent.verticalCenter}
                    width:24*root.unit;height:24*root.unit
                    visible:row.modelData.type!=="heading"&&!!row.modelData.description
                    text:VLCIcons.info
                    Accessible.name:qsTr("About %1").arg(row.modelData.label)
                    contentItem:Text {text:parent.text;color:root.textColor;font.family:VLCIcons.fontFamily;font.pixelSize:18*root.unit;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                    background:Rectangle {radius:4*root.unit;color:parent.hovered?root.panel:"transparent"}
                    onClicked:{root.infoOption=row.modelData;optionInfo.open()}
                }
                Loader {
                    id:optionEditor
                    anchors {right:parent.right;verticalCenter:parent.verticalCenter}
                    width:Math.min(300*root.unit,parent.width*.46)
                    active:modelData.type!=="heading"
                    property var opt:row.modelData
                    sourceComponent:opt.hotkey?shortcutControl:opt.editor==="audioFilters"?audioFiltersControl:opt.editor==="range"?rangeControl:opt.editor==="fontSize"?fontSizeControl:opt.color?fieldControl:(opt.type==="bool"?toggleControl:(opt.choices&&opt.choices.length>0?choiceControl:fieldControl))
                }
            }
            footer:Column {
                width:settingList.width;spacing:14*root.unit
                SecondaryButton {visible:root.category===10&&!search.text;text:qsTr("Rescan plugins");onClicked:MainCtx.rescanRedesignPlugins()}
                SecondaryButton {visible:root.category===2&&!search.text;text:qsTr("Clear playback history");onClicked:historyConfirm.open()}
                SecondaryButton {visible:root.category===3&&!search.text;text:qsTr("Reset all shortcuts");onClicked:root.resetShortcuts()}
                SecondaryButton {visible:root.category===5&&!search.text;text:qsTr("Reset picture");onClicked:root.resetPicture()}
                Loader {width:parent.width;active:root.category===12&&!search.text;sourceComponent:extensionsList}

                SecondaryButton {
                    objectName:"redesignShortcutEditorOpen"
                    visible:root.category===3&&!root.shortcutsOnly&&!search.text
                    text:qsTr("Edit Keyboard Shortcuts");onClicked:{root.shortcutsOnly=true;search.text="";settingList.positionViewAtBeginning()}
                }
                SecondaryButton {
                    visible:root.shortcutsOnly
                    text:qsTr("Back to Controls");onClicked:{root.shortcutsOnly=false;root.category=3;search.text="";settingList.positionViewAtBeginning()}
                }
                Rectangle {visible:!root.shortcutsOnly;width:parent.width;height:1;color:root.panel}
                C.Button {
                    objectName:"redesignAdvancedOptionsToggle"
                    visible:!root.shortcutsOnly
                    text:root.advanced?qsTr("Hide Advanced Options"):qsTr("Advanced Options")
                    padding:0
                    Accessible.role:Accessible.Button
                    Accessible.description:root.advanced?qsTr("Expanded"):qsTr("Collapsed")
                    background:Rectangle {color:"transparent";radius:4*root.unit;border.color:parent.visualFocus?root.accent:"transparent"}
                    contentItem:Row {
                        spacing:8*root.unit
                        Text {text:"\u203A";rotation:root.advanced?90:0;color:root.textColor;font.pixelSize:20*root.unit;anchors.verticalCenter:parent.verticalCenter;Behavior on rotation {NumberAnimation {duration:120}}}
                        Text {text:parent.parent.text;color:parent.parent.hovered?root.titleColor:root.textColor;font.pixelSize:16*root.unit;anchors.verticalCenter:parent.verticalCenter}
                    }
                    onClicked: {const y=settingList.contentY;root.advanced=!root.advanced;Qt.callLater(()=>settingList.contentY=Math.min(y,Math.max(0,settingList.contentHeight-settingList.height)))}
                }
                Flow {
                    width:parent.width;spacing:8*root.unit
                    SecondaryButton {visible:root.category===0&&root.advanced&&!search.text;text:qsTr("Reset all settings");onClicked:resetConfirm.open()}
                    SecondaryButton {visible:root.category===12&&root.advanced&&!search.text;text:qsTr("Native add-on tools…");onClicked:DialogsProvider.pluginDialog()}
                    SecondaryButton {text:qsTr("All VLC settings & reset…");onClicked:DialogsProvider.classicPrefsDialog()}
                }
                Text {text:qsTr("Some changes take effect the next time a file opens or VLC restarts.");width:parent.width;wrapMode:Text.WordWrap;color:root.mutedColor;font.pixelSize:12*root.unit}
                Item {width:1;height:24*root.unit}
            }
        }
        Rectangle {
            anchors {left:parent.left;right:parent.right;bottom:parent.bottom;leftMargin:(root.shortcutsOnly?8:216)*root.unit;rightMargin:8*root.unit;bottomMargin:8*root.unit}
            height:58*root.unit;color:root.paper
            Text {anchors {left:parent.left;right:buttons.left;verticalCenter:parent.verticalCenter;leftMargin:24*root.unit;rightMargin:12*root.unit} text:root.failure;wrapMode:Text.WordWrap;color:"#c14545";font.pixelSize:12*root.unit}
            Row {
                id:buttons;spacing:8*root.unit
                anchors {right:parent.right;verticalCenter:parent.verticalCenter;rightMargin:24*root.unit}
                SecondaryButton {text:qsTr("Cancel");onClicked:root.close()}
                C.Button {
                    objectName:"redesignSettingsSave"
                    text:qsTr("Save");width:88*root.unit;height:34*root.unit
                    background:Rectangle {radius:6*root.unit;color:root.accent}
                    contentItem:Text {text:parent.text;color:VLCStyle.onAccent(root.accent);horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;font.pixelSize:14*root.unit}
                    onClicked:root.save()
                }
            }
        }
    }

    C.Popup {
        id:optionInfo
        objectName:"redesignOptionInfo"
        parent:root.contentItem
        width:Math.min(480*root.unit,parent.width-32*root.unit)
        height:Math.min(infoColumn.implicitHeight+2*padding,root.height-32*root.unit)
        x:(parent.width-width)/2;y:(parent.height-height)/2
        padding:20*root.unit;focus:true;modal:false
        closePolicy:C.Popup.CloseOnEscape|C.Popup.CloseOnPressOutside
        background:Rectangle {color:root.panel;radius:8*root.unit;border.color:root.textColor}
        contentItem:ColumnLayout {
            id:infoColumn
            spacing:12*root.unit
            Text {Layout.fillWidth:true;text:root.infoOption.label||"";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.titleColor;font.pixelSize:16*root.unit}
            Flickable {
                objectName:"redesignInfoScroll"
                Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:0
                implicitHeight:infoDescription.implicitHeight
                contentWidth:width;contentHeight:infoDescription.implicitHeight;clip:true
                C.ScrollBar.vertical:C.ScrollBar {}
                Text {id:infoDescription;width:parent.width;text:root.infoOption.description||"";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:root.textColor;font.pixelSize:14*root.unit}
            }
            SecondaryButton {text:qsTr("Close");onClicked:optionInfo.close()}
        }
    }
    Component {
        id:shortcutControl
        RowLayout {
            height:34*root.unit;spacing:6*root.unit
            C.TextField {
                id:capture
                objectName:"redesignShortcut-"+opt.key
                Layout.fillWidth:true;height:34*root.unit
                readOnly:true;property bool capturing:false
                text:capturing?qsTr("Press a shortcut…"):String(root.value(opt))
                placeholderText:qsTr("No shortcut")
                font.pixelSize:14*root.unit;color:root.textColor;leftPadding:12*root.unit
                background:Rectangle {color:root.field;radius:6*root.unit;border.color:capture.capturing?root.accent:root.panel;border.width:2}
                TapHandler {onTapped:{capture.capturing=true;capture.forceActiveFocus(Qt.TabFocusReason)}}
                onActiveFocusChanged:if(!activeFocus)capturing=false
                Keys.priority:Keys.BeforeItem
                Keys.onPressed:event=>{
                    if(!capturing)return
                    event.accepted=true
                    if(event.key===Qt.Key_Escape){capturing=false;return}
                    if([Qt.Key_Control,Qt.Key_Shift,Qt.Key_Alt,Qt.Key_Meta].includes(event.key))return
                    const shortcut=MainCtx.redesignHotkey(event.key,event.modifiers)
                    if(shortcut){capturing=false;root.assignShortcut(opt,shortcut)}
                }
                Accessible.name:opt.label
            }
            SecondaryButton {text:"×";width:34*root.unit;leftPadding:0;rightPadding:0;Accessible.name:qsTr("Clear shortcut");onClicked:root.edit(opt,"")}
        }
    }
    C.Dialog {
        id:shortcutConflict;parent:C.Overlay.overlay
        objectName:"redesignShortcutConflict"
        modal:true;focus:true;width:480*root.unit
        x:(parent.width-width)/2;y:(parent.height-height)/2
        padding:24*root.unit
        background:Rectangle {color:root.paper;radius:14*root.unit;border.color:root.panel}
        contentItem:ColumnLayout {
            spacing:18*root.unit
            Text {text:qsTr("Shortcut already in use");color:root.titleColor;font.pixelSize:16*root.unit}
            Text {text:root.pendingShortcut.conflicts?qsTr("%1 is assigned to %2. Replace those assignments?").arg(root.pendingShortcut.shortcut).arg(root.pendingShortcut.conflicts.map(item=>item.label).join(", ")):"";color:root.textColor;wrapMode:Text.WordWrap;Layout.fillWidth:true;font.pixelSize:14*root.unit}
            RowLayout {
                Item {Layout.fillWidth:true}
                SecondaryButton {text:qsTr("Cancel");onClicked:shortcutConflict.close()}
                SecondaryButton {objectName:"redesignShortcutReplace";text:qsTr("Replace");onClicked:{
                    if (!root.pendingShortcut.conflicts) return
                    root.pendingShortcut.conflicts.forEach(opt=>root.edit(opt,String(root.value(opt)).split("\t").filter(key=>key!==root.pendingShortcut.shortcut).join("\t")))
                    root.edit(root.pendingShortcut.option,root.pendingShortcut.shortcut)
                    shortcutConflict.close()
                }}
            }
        }
    }
    C.Popup {
        id:historyConfirm;parent:root.contentItem;modal:true;anchors.centerIn:parent;width:Math.min(440*root.unit,root.width-32);padding:20
        background:Rectangle {color:root.panel;radius:8}
        contentItem:ColumnLayout {
            Text {text:qsTr("Clear playback history?");color:root.titleColor;font.pixelSize:18*root.unit;Layout.fillWidth:true;wrapMode:Text.WordWrap}
            Text {text:qsTr("Your media files, playlists and bookmarks will be kept.");color:root.textColor;Layout.fillWidth:true;wrapMode:Text.WordWrap}
            RowLayout {SecondaryButton {text:qsTr("Cancel");onClicked:historyConfirm.close()} SecondaryButton {text:qsTr("Clear history");onClicked:{MainCtx.clearRedesignHistory();historyConfirm.close()}}}
        }
    }
    C.Popup {
        id:resetConfirm;parent:root.contentItem;modal:true;focus:true;width:Math.min(480*root.unit,root.width-32);x:(root.width-width)/2;y:(root.height-height)/2
        background:Rectangle {color:root.panel;radius:8*root.unit}
        contentItem:Column {spacing:18*root.unit
            Text {width:parent.width;text:qsTr("Reset all preferences to their native defaults? Click Save in Settings to apply. Your library and bookmarks are kept.");wrapMode:Text.WordWrap;color:root.titleColor}
            Row {spacing:12*root.unit;SecondaryButton {text:qsTr("Cancel");onClicked:resetConfirm.close()} SecondaryButton {text:qsTr("Reset");onClicked:{root.resetAll();resetConfirm.close()}}}
        }
    }
    Component {
        id:rangeControl
        ColumnLayout {
            spacing:0
            Text {text:(Number(root.value(opt))/opt.factor).toFixed(opt.step<1?1:0)+opt.suffix;color:root.titleColor;font.pixelSize:12*root.unit;Layout.alignment:Qt.AlignRight}
            C.Slider {Layout.fillWidth:true;from:opt.from;to:opt.to;stepSize:opt.step;value:Number(root.value(opt))/opt.factor
                handle:Rectangle {x:parent.leftPadding+parent.visualPosition*(parent.availableWidth-width);y:parent.topPadding+(parent.availableHeight-height)/2;width:18*root.unit;height:width;radius:width/2;color:root.accent;border.color:root.titleColor;border.width:parent.activeFocus?1:0}
                background:Rectangle {x:parent.leftPadding;y:parent.topPadding+(parent.availableHeight-height)/2;width:parent.availableWidth;height:4*root.unit;radius:2*root.unit;color:root.panel;Rectangle {width:parent.parent.visualPosition*parent.width;height:parent.height;radius:parent.radius;color:root.accent}}
                onMoved:root.edit(opt,opt.type==="int"?Math.round(value*opt.factor):value*opt.factor)}
        }
    }
    Component {
        id:fontSizeControl
        RowLayout {
            spacing:6
            C.ComboBox {model:[qsTr("Relative"),qsTr("Pixels")];currentIndex:Number(root.value(opt))>0?1:0;Layout.fillWidth:true
                onActivated:root.edit(opt,currentIndex===0?0:32)}
            C.SpinBox {from:Number(root.value(opt))>0?1:10;to:500;editable:true
                value:Number(root.value(opt))>0?Number(root.value(opt)):Number(root.value(root.find("sub-text-scale")))
                onValueModified:{if(Number(root.value(opt))>0)root.edit(opt,value);else root.edit(root.find("sub-text-scale"),value)}}
            Text {text:Number(root.value(opt))>0?"px":"%";color:root.textColor}
        }
    }
    Component {
        id:extensionsList
        Column {
            width:settingList.width;spacing:12*root.unit
            ServicesDiscoveryModel {id:addons;ctx:MainCtx;stateFilter:showCatalog.checked?ServicesDiscoveryModel.STATE_NONE:ServicesDiscoveryModel.STATE_INSTALLED}
            Text {text:qsTr("Add-ons");color:root.titleColor;font.pixelSize:16*root.unit}
            RowLayout {
                width:parent.width
                C.TextField {placeholderText:qsTr("Search add-ons");Layout.fillWidth:true;onTextChanged:addons.searchPattern=text}
                C.CheckBox {id:showCatalog;text:qsTr("Catalog");onClicked:if(checked)addons.loadFromDefaultRepository()}
                SecondaryButton {text:qsTr("Import package…");onClicked:{const path=MainCtx.pickRedesignPath("redesign-addon-file","");if(path){showCatalog.checked=true;addons.loadFromExternalRepository(MainCtx.redesignLocalFileUrl(path))}}}
            }
            Repeater {
                model:addons
                delegate:Column {
                    required property var model;required property int index;width:settingList.width;spacing:6
                    RowLayout {
                        width:parent.width
                        Text {text:parent.parent.model.name;textFormat:Text.PlainText;color:root.titleColor;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                        SecondaryButton {text:parent.parent.model.state===ServicesDiscoveryModel.STATE_INSTALLED?qsTr("Remove"):qsTr("Install")
                            enabled:parent.parent.model.manageable && (parent.parent.model.state===ServicesDiscoveryModel.STATE_INSTALLED||parent.parent.model.state===ServicesDiscoveryModel.STATE_NOTINSTALLED)
                            onClicked:{if(parent.parent.model.state===ServicesDiscoveryModel.STATE_INSTALLED)addons.removeService(parent.parent.index);else addons.installService(parent.parent.index)}}
                    }
                    Text {width:parent.width;text:parent.model.summary||"";textFormat:Text.PlainText;color:root.textColor;wrapMode:Text.WordWrap;font.pixelSize:12*root.unit}
                }
            }
            Text {visible:addons.count===0;text:addons.loading?qsTr("Loading add-ons…"):qsTr("No installed add-ons.");color:root.textColor}
        }
    }

    Component {
        id:toggleControl
        Item {
            height:32*root.unit
            C.Button {
                anchors.right:parent.right;width:68*root.unit;height:32*root.unit
                padding:0
                property bool on:root.value(opt)
                Accessible.name:opt.label
                Accessible.role:Accessible.CheckBox
                Accessible.checked:on
                background:Rectangle {color:parent.on?(root.light?Qt.alpha(root.accent,.22):Qt.darker(root.accent,3.8)):root.panel;radius:6*root.unit;border.width:2;border.color:parent.visualFocus?root.accent:root.field}
                contentItem:Item {
                    Rectangle {x:parent.parent.on?36*root.unit:3*root.unit;y:3*root.unit;width:26*root.unit;height:26*root.unit;radius:3*root.unit;color:parent.parent.on?Qt.darker(root.accent,1.2):root.knobColor}
                    Text {x:parent.parent.on?4*root.unit:34*root.unit;width:30*root.unit;height:32*root.unit;text:parent.parent.on?qsTr("ON"):qsTr("OFF");color:parent.parent.on?(root.light?Qt.darker(root.accent,1.4):root.accent):root.mutedColor;font.pixelSize:12*root.unit;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                }
                onClicked:root.edit(opt,!on)
            }
        }
    }
    Component {
        id:audioFiltersControl
        Column {
            readonly property var known:[{key:"equalizer",label:qsTr("Equalizer")},{key:"compressor",label:qsTr("Dynamic range compressor")},{key:"spatializer",label:qsTr("Spatializer")},{key:"normvol",label:qsTr("Volume normalization")},{key:"stereowiden",label:qsTr("Stereo widening")}]
            width:parent.width;spacing:2*root.unit
            Repeater {model:parent.known
                C.CheckBox {
                    required property var modelData
                    text:modelData.label;font.pixelSize:13*root.unit
                    checked:String(root.value(opt)).split(":").includes(modelData.key)
                    onClicked:{let filters=String(root.value(opt)).split(":").filter(s=>s.length&&s!==modelData.key);if(checked)filters.push(modelData.key);root.edit(opt,filters.join(":"))}
                }
            }
            C.TextField {
                width:parent.width;height:30*root.unit;color:root.textColor
                text:String(root.value(opt));placeholderText:qsTr("Additional filter names")
                onTextEdited:root.edit(opt,text)
                background:Rectangle {color:root.field;radius:4*root.unit}
                Accessible.name:qsTr("Complete native audio filter chain")
            }
        }
    }
    Component {
        id:choiceControl
        C.ComboBox {
            id:combo
            objectName:"redesignSettingChoice"
            height:34*root.unit;implicitHeight:34*root.unit
            model:opt.choices;textRole:"label";valueRole:"value"
            currentIndex:opt.choices.findIndex(c=>c.value===root.value(opt))
            font.pixelSize:14*root.unit
            background:Rectangle {color:root.panel;radius:6*root.unit;border.color:combo.visualFocus?root.accent:root.field;border.width:2}
            contentItem:Text {text:combo.displayText;leftPadding:14*root.unit;rightPadding:4*root.unit;color:root.textColor;font.pixelSize:14*root.unit;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
            C.ToolTip.visible:hovered&&contentItem.truncated;C.ToolTip.text:displayText;C.ToolTip.delay:500
            // Figma dropdown: 24px chevron, rounded list, current row highlighted with an accent check
            indicator:Image {x:combo.width-width-10*root.unit;y:(combo.height-height)/2;width:24*root.unit;height:width;sourceSize:Qt.size(width,height);source:VLCStyle.iconUrl("settings-imgFrame.svg",root.textColor);rotation:combo.popup.visible?180:0}
            rightPadding:38*root.unit
            popup:C.Popup {
                y:combo.height+4*root.unit;width:combo.width;padding:4*root.unit
                implicitHeight:Math.min(contentItem.implicitHeight+topPadding+bottomPadding,320*root.unit)
                background:Rectangle {color:root.panel;radius:8*root.unit;border.color:root.field;border.width:2}
                contentItem:ListView {
                    clip:true;implicitHeight:contentHeight
                    model:combo.popup.visible?combo.delegateModel:null
                    currentIndex:combo.highlightedIndex
                    C.ScrollIndicator.vertical:C.ScrollIndicator {}
                }
            }
            delegate:C.ItemDelegate {
                required property int index
                width:combo.width-8*root.unit;height:36*root.unit
                highlighted:combo.highlightedIndex===index
                leftPadding:12*root.unit;rightPadding:12*root.unit
                contentItem:RowLayout {
                    spacing:8*root.unit
                    Text {text:combo.textAt(parent.parent.index);color:parent.parent.index===combo.currentIndex?root.titleColor:root.textColor;font.pixelSize:14*root.unit;elide:Text.ElideRight;Layout.fillWidth:true}
                    Text {text:"✓";visible:parent.parent.index===combo.currentIndex;color:root.accent;font.pixelSize:16*root.unit}
                }
                background:Rectangle {color:parent.highlighted||parent.index===combo.currentIndex?root.hoverColor:"transparent";radius:4*root.unit}
            }
            onActivated:root.edit(opt, opt.choices[currentIndex].value)
            Accessible.name:opt.label
        }
    }
    Component {
        id:fieldControl
        Item {
            height:34*root.unit
            C.TextField {
                anchors { left:parent.left; right:picker.visible?picker.left:parent.right; top:parent.top;bottom:parent.bottom;rightMargin:picker.visible?8*root.unit:0 }
                leftPadding:14*root.unit
                // -1 on the *-track options means "pick automatically"; floats are stored
                // in single precision, so show at most four decimals (12.8, not 12.800000190734863)
                readonly property bool automatic:/-track$/.test(opt.key)&&Number(root.value(opt))===-1
                placeholderText:automatic?qsTr("Auto"):""
                text:opt.color ? "#" + ("000000" + Number(root.value(opt)).toString(16)).slice(-6).toUpperCase()
                    : automatic ? "" : opt.type==="float" && isFinite(Number(root.value(opt))) ? String(+Number(root.value(opt)).toFixed(4)) : String(root.value(opt)??"")
                color:root.textColor;font.pixelSize:14*root.unit
                echoMode:opt.password?TextInput.Password:TextInput.Normal
                background:Rectangle {color:root.field;radius:6*root.unit;border.color:parent.activeFocus?root.accent:root.panel;border.width:2}
                onTextEdited:root.edit(opt,opt.color?parseInt(text.replace(/^#/,""),16):(opt.type==="int"||opt.type==="float"?(text.trim()===""&&/-track$/.test(opt.key)?-1:Number(text)):text))
                Accessible.name:opt.label
            }
            SecondaryButton {
                id:picker
                anchors.right:parent.right
                width:opt.color?34*root.unit:94*root.unit
                visible:!!opt.color||!!opt.path
                text:opt.color?"":qsTr("Browse")
                Accessible.name:opt.color?qsTr("Choose %1").arg(opt.label):qsTr("Browse for %1").arg(opt.label)
                Rectangle {
                    anchors.centerIn:parent;visible:!!opt.color
                    width:22*root.unit;height:width;radius:2*root.unit
                    color: "#" + ("000000" + Number(root.value(opt)).toString(16)).slice(-6)
                }
                onClicked: {
                    if (opt.color) {
                        const hex = MainCtx.pickRedesignColor("#" + ("000000" + Number(root.value(opt)).toString(16)).slice(-6))
                        if (hex) root.edit(opt,parseInt(hex.slice(1),16))
                    } else {
                        const path = MainCtx.pickRedesignPath(opt.key,String(root.value(opt)??""))
                        if (path) root.edit(opt,path)
                    }
                }
            }
        }
    }
}
