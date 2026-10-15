// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as C
import VLC.MainInterface
import VLC.Player
import VLC.Playlist
import VLC.Dialogs
import VLC.Style
import VLC.Util

C.Popup {
    id: root
    objectName: "redesignLiveControls"
    property int currentTab: 3
    property bool legacyTools: false
    property int revision: 0
    property var options: []
    property string status: ""
    property bool showSubtitleTimeline: MainCtx.redesignPresentation.cues
    readonly property var subtitleCues: MainCtx.redesignTimelineCues
    readonly property real unit: VLCStyle.scale
    readonly property bool dark: colorContext.palette?.isDark ?? true
    readonly property color paper: dark ? "#0e0e0e" : "#f8f8f8"
    readonly property color field: dark ? "#090909" : "#ffffff"
    readonly property color ink: dark ? "#8f8f8f" : "#333333"
    readonly property color edge: dark ? "#222222" : "#cccccc"
    readonly property color title: dark ? "#c6c6c6" : "#242424"
    readonly property color line: dark ? "#373737" : "#cccccc"
    readonly property color track: dark ? "#292929" : "#dcdcdc"
    readonly property color hover: dark ? "#222222" : "#dddddd"
    readonly property color pressed: dark ? "#292929" : "#d2d2d2"
    readonly property color muted: dark ? "#6b6b6b" : "#777777"
    // same surfaces as RedesignSettings so the panel and dialog controls read as one set
    readonly property color panel: dark ? "#191919" : "#eaeaea"
    readonly property color knob: dark ? "#404040" : "#bdbdbd"
    readonly property ColorContext colorContext: ColorContext { colorSet: ColorContext.Window }
    width: Math.min(400 * unit, parent.width)
    height: Math.min(945 * unit, parent.height - 90 * unit)
    padding: 0
    focus: true
    modal: false
    closePolicy: C.Popup.CloseOnEscape | C.Popup.CloseOnPressOutside
    palette.window: paper
    palette.base: field
    palette.button: field
    palette.text: ink
    palette.buttonText: ink
    palette.highlight: colorContext.accent
    palette.highlightedText: VLCStyle.onAccent(colorContext.accent)
    onOpened: { options = MainCtx.redesignConfigOptions(); revision++; legacyTools = false; if (showSubtitleTimeline) MainCtx.requestRedesignSubtitleTimeline() }
    onClosed: if(!MainCtx.redesignPresentation.cues)MainCtx.cancelRedesignSubtitleTimeline()
    Timer { id: refreshTimeline; interval: 150; onTriggered: if (root.opened && root.showSubtitleTimeline) MainCtx.requestRedesignSubtitleTimeline() }
    Connections {
        target: Player.subtitleTracks
        function onDataChanged() { if (root.opened && root.showSubtitleTimeline) refreshTimeline.restart() }
        function onRowsInserted() { if (root.opened && root.showSubtitleTimeline) refreshTimeline.restart() }
        function onRowsRemoved() { if (root.opened && root.showSubtitleTimeline) refreshTimeline.restart() }
    }

    function option(key) { return options.find(option => option.key === key) }
    function showSubtitleImport() { batchSubtitles.open() }
    function closeSubtitleImport() { batchSubtitles.close() }
    function read(key) { const updated = revision; return MainCtx.redesignLiveValue(key) }
    function equalizerBands() {
        let values = String(read("equalizer-bands") ?? "").trim().split(/\s+/).map(Number)
        if (values.length !== 10 || values.some(value => !isFinite(value))) return Array(10).fill(0)
        return values
    }
    function setEqualizerBand(index, value) { let values = equalizerBands(); values[index] = value; write("equalizer-bands", values.map(value => value.toFixed(1)).join(" ")) }
    function write(key, value) {
        const values = {}; values[key] = value
        status = MainCtx.applyRedesignLiveOptions(values) ? "" : qsTr("That value is outside the supported range.")
        revision++
    }
    function snapshot() {
        const values = {}
        const common = currentTab===3 ? ["sub-language","sub-track","sub-filter","freetype-font","freetype-bold","freetype-color","freetype-opacity","sub-text-scale","freetype-fontsize","freetype-outline-color","freetype-outline-thickness","freetype-shadow-color","freetype-shadow-distance","freetype-background-opacity","freetype-background-color","sub-margin","subsdelay-factor"]
            : currentTab===2 ? ["audio-language","audio-track","audio-filter"]
            : currentTab===1 ? ["video-filter","aspect-ratio","crop","zoom","deinterlace","deinterlace-mode"]
            : ["restore-playback-pos","video-on-top","qt-redesign-show-waveform","qt-redesign-show-cues","qt-redesign-show-metadata"]
        const filterModules=currentTab===2?["equalizer","compressor","spatializer","stereowiden","normvol","scaletempo","headphone","karaoke","chorus_flanger"]:currentTab===1?["adjust","transform","sharpen","gradfun","grain"]:[]
        const keys=new Set(common.concat(options.filter(o=>filterModules.includes(o.module)).map(o=>o.key)))
        keys.forEach(key=>{if(option(key))values[key]=read(key)})
        if(currentTab===3){values._subtitleDelay=Player.subtitleDelayMS;values._secondarySubtitleDelay=Player.secondarySubtitleDelayMS;values._subtitleFPS=Player.subtitleFPS}
        if(currentTab===2){values._audioDelay=Player.audioDelayMS;values._stereoMode=Player.audioStereoMode.currentRow;values._audioDevice=Player.audioDevices.selectedId}
        if(currentTab===1){values._aspect=Player.aspectRatio.currentRow;values._crop=Player.crop.currentRow;values._zoom=Player.zoom.currentRow;values._deinterlace=Player.deinterlace.currentRow;values._deinterlaceMode=Player.deinterlaceMode.currentRow}
        if(currentTab===0)values._rate=Player.rate
        return values
    }
    function savePreset() {
        const name = presetName.text.trim()
        if (!name) return
        let saved = {}
        try { saved = JSON.parse(MainCtx.settingValue("Redesign/live-presets-" + currentTab, "{}")) } catch (error) {}
        saved[name] = snapshot()
        MainCtx.setSettingValue("Redesign/live-presets-" + currentTab, JSON.stringify(saved))
        revision++
    }
    function presetNames() {
        const updated = revision
        try { return Object.keys(JSON.parse(MainCtx.settingValue("Redesign/live-presets-" + currentTab, "{}"))) } catch (error) { return [] }
    }
    function applyPreset(name) {
        let saved = {}
        try { saved = JSON.parse(MainCtx.settingValue("Redesign/live-presets-" + currentTab, "{}"))[name] } catch (error) { return }
        const values = Object.assign({}, saved)
        Object.keys(values).filter(k=>k.startsWith("_")).forEach(k=>delete values[k])
        if (!MainCtx.applyRedesignLiveOptions(values)) { status = qsTr("This preset contains unsupported values."); return }
        if (saved._subtitleDelay !== undefined) Player.subtitleDelayMS = saved._subtitleDelay
        if (saved._audioDelay !== undefined) Player.audioDelayMS = saved._audioDelay
        if (saved._rate !== undefined) Player.rate = saved._rate
        if(saved._secondarySubtitleDelay!==undefined)Player.secondarySubtitleDelayMS=saved._secondarySubtitleDelay
        if(saved._subtitleFPS!==undefined)Player.subtitleFPS=saved._subtitleFPS
        const choices={_stereoMode:Player.audioStereoMode,_aspect:Player.aspectRatio,_crop:Player.crop,_zoom:Player.zoom,_deinterlace:Player.deinterlace,_deinterlaceMode:Player.deinterlaceMode}
        Object.keys(choices).forEach(k=>{if(saved[k]!==undefined&&saved[k]>=0)choices[k].toggleIndex(saved[k])})
        if(saved._audioDevice!==undefined&&!Player.audioDevices.selectById(saved._audioDevice))status=qsTr("The saved audio device is unavailable. Your current device is kept.")
        revision++
    }
    background: Rectangle { color: root.paper; radius: 12 * root.unit; border.color: root.edge }
    contentItem: ColumnLayout {
        spacing: 0
        C.TabBar {
            id: tabs
            Layout.fillWidth: true
            Layout.preferredHeight: 50 * root.unit
            currentIndex: root.currentTab
            onCurrentIndexChanged: root.currentTab = currentIndex
            background: Rectangle { color: root.field; radius: 12 * root.unit }
            Repeater {
                // Figma tabs pair each label with the matching settings-category icon
                model: [{label: qsTr("Playback"), icon: "10"}, {label: qsTr("Video"), icon: "11"}, {label: qsTr("Audio"), icon: "12"}, {label: qsTr("Subtitles"), icon: "13"}]
                C.TabButton {
                    id: tabButton
                    required property int index
                    required property var modelData
                    readonly property bool active: root.currentTab === index
                    width: root.width / 4; height: 50 * root.unit
                    implicitWidth: width; implicitHeight: height
                    leftPadding: 0; rightPadding: 0; topPadding: 0; bottomPadding: 0
                    font.pixelSize: 13 * root.unit; text: modelData.label
                    contentItem: Item {
                        Row {
                            // centred on the whole tab; the icon keeps one source (a recoloured
                            // source reloads asynchronously and can vanish on tab switches)
                            x: (tabButton.width - width) / 2; y: (tabButton.height - height) / 2
                            spacing: 6 * root.unit
                            Image { width: 18 * root.unit; height: width; sourceSize: Qt.size(width, height); anchors.verticalCenter: parent.verticalCenter; opacity: tabButton.active ? 1 : .7; source: VLCStyle.iconUrl("settings-imgFrame" + tabButton.modelData.icon + ".svg", root.title) }
                            Text { text: tabButton.text; color: tabButton.active ? root.title : root.ink; font: tabButton.font; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                    background: Rectangle {
                        color: "transparent"
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: root.colorContext.accent; visible: root.currentTab === index }
                    }
                }
            }
        }
        C.ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; visible: !root.legacyTools; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 0
                Section {
                    title: qsTr("Playback"); visible: root.currentTab === 0
                    ValueSlider { title: qsTr("Speed"); from: 0.25; to: 4; step: 0.05; current: Player.rate; suffix: "×"; onEdited: value => { Player.rate = value } }
                    Action { text: qsTr("Normal speed"); onClicked: Player.rate = 1 }
                    RowLayout {
                        Action { text: (Player.reversePlaybackActive||Player.rate<0)?qsTr("Stop reverse"):qsTr("Reverse");enabled:Player.seekable&&Player.hasVideoOutput;onClicked: Player.reverse() }
                        Action { text: qsTr("A–B repeat"); onClicked: Player.toggleABloopState() }
                    }
                    Text {visible:Player.reversePlaybackActive;text:qsTr("Reverse preview · audio paused");color:root.ink;Layout.fillWidth:true}
                    OptionRow { key: "restore-playback-pos"; title: qsTr("Resume playback") }
                    Action { text: root.showSubtitleTimeline ? qsTr("Hide subtitle timeline") : qsTr("Subtitle timeline"); onClicked: { root.showSubtitleTimeline = !root.showSubtitleTimeline; root.write("qt-redesign-show-cues",root.showSubtitleTimeline); if (root.showSubtitleTimeline) MainCtx.requestRedesignSubtitleTimeline(); else MainCtx.cancelRedesignSubtitleTimeline() } }
                    Action { visible: root.showSubtitleTimeline; text: qsTr("Refresh timeline"); enabled: !MainCtx.redesignTimelineBusy; onClicked: MainCtx.requestRedesignSubtitleTimeline() }
                    Text { visible: root.showSubtitleTimeline && MainCtx.redesignTimelineMessage.length > 0; text: MainCtx.redesignTimelineMessage; textFormat: Text.PlainText; color: root.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 * root.unit }
                    C.ScrollView {
                        visible: root.showSubtitleTimeline && root.subtitleCues.length > 0
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300 * root.unit
                        contentWidth: availableWidth
                        C.ScrollBar.horizontal.policy: C.ScrollBar.AlwaysOff
                        clip: true
                        ListView {
                            model: root.showSubtitleTimeline ? root.subtitleCues : []
                            clip: true
                            delegate: C.ItemDelegate {
                                id: timelineCue
                                objectName: "redesignTimelineCue"
                                required property var modelData
                                readonly property real effectiveStart: {
                                    const fps = Player.subtitleFPS, delay = Player.subtitleDelayMS
                                    return MainCtx.redesignSubtitleCueTime(modelData.start)
                                }
                                width: ListView.view.width
                                implicitHeight: Math.max(40 * root.unit, contentItem.implicitHeight + 16 * root.unit)
                                Accessible.name: effectiveStart.toFixed(1) + " s " + modelData.text
                                contentItem: Text { text: timelineCue.effectiveStart.toFixed(1) + " s   " + timelineCue.modelData.text; textFormat: Text.PlainText; color: root.ink; wrapMode: Text.WordWrap; font.pixelSize: 12 * root.unit }
                                background: Rectangle { color: timelineCue.hovered ? root.edge : "transparent"; radius: 4 * root.unit }
                                onClicked: { const duration = Player.length.toSeconds(); if (duration > 0) Player.position = Math.max(0, Math.min(1, MainCtx.redesignSubtitleCueTime(modelData.start) / duration)) }
                            }
                        }
                    }
                    OptionRow { key: "video-on-top"; title: qsTr("Always on top") }
                    RowLayout {
                        Text { text: qsTr("Sleep timer"); color: root.ink; Layout.fillWidth: true; font.pixelSize: 14 * root.unit }
                        C.SpinBox { id: sleepMinutes; from: 1; to: 1440; value: 30; editable: true; Layout.preferredWidth: 110 * root.unit }
                        Action { text: MainCtx.redesignSleepTimerActive ? qsTr("Cancel") : qsTr("Start"); onClicked: { MainCtx.setRedesignSleepTimer(MainCtx.redesignSleepTimerActive ? 0 : sleepMinutes.value) } }
                    }
                    Text { text: qsTr("Stops playback after the selected number of minutes."); color: root.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 * root.unit }
                    RowLayout {
                        Text {text:qsTr("Show waveform on the seek bar");color:root.ink;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:13*root.unit}
                        Item {Layout.preferredWidth:170*root.unit;implicitHeight:waveformToggle.implicitHeight
                            Toggle {id:waveformToggle;text:qsTr("Show waveform on the seek bar");checked:MainCtx.redesignPresentation.waveform;onClicked:root.write("qt-redesign-show-waveform",checked)}}
                    }
                    RowLayout {
                        Text {text:qsTr("Show media information");color:root.ink;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:13*root.unit}
                        Item {Layout.preferredWidth:170*root.unit;implicitHeight:metadataToggle.implicitHeight
                            Toggle {id:metadataToggle;text:qsTr("Show media information");checked:MainCtx.redesignPresentation.metadata;onClicked:root.write("qt-redesign-show-metadata",checked)}}
                    }
                    Action { text: qsTr("Full media information…"); onClicked: DialogsProvider.mediaInfoDialog() }
                }
                Section {
                    title: qsTr("Video Tracks"); visible: root.currentTab === 1
                    Tracks { tracks: Player.videoTracks }
                    Action { text: qsTr("Add video track"); onClicked: DialogsProvider.loadVideoFile() }
                }
                Section {
                    title: qsTr("Video"); visible: root.currentTab === 1
                    Choice { title: qsTr("Aspect ratio"); choices: Player.aspectRatio }
                    Choice { title: qsTr("Crop"); choices: Player.crop }
                    Choice { title: qsTr("Zoom"); choices: Player.zoom }
                    Choice { title: qsTr("Deinterlace"); choices: Player.deinterlace }
                    Choice { title: qsTr("Deinterlace mode"); choices: Player.deinterlaceMode }
                }
                Repeater {
                    model: [{key:"adjust",title:qsTr("Colors")},{key:"transform",title:qsTr("Flip & Rotate")},{key:"sharpen",title:qsTr("Sharpen")},{key:"gradfun",title:qsTr("Banding removal")},{key:"grain",title:qsTr("Film grain")}]
                    FilterSection { required property var modelData; visible: root.currentTab === 1; filter: modelData.key; title: modelData.title; audio: false }
                }
                Section {
                    title: qsTr("Audio Tracks"); visible: root.currentTab === 2
                    Tracks { tracks: Player.audioTracks }
                    Choice {title:qsTr("Output device");choices:Player.audioDevices}
                    Action { text: qsTr("Add audio track"); onClicked: DialogsProvider.loadAudioFile() }
                }
                Section {
                    title: qsTr("Defaults"); visible: root.currentTab === 2
                    OptionRow { key: "audio-track"; title: qsTr("Audio track") }
                    OptionRow { key: "audio-language"; title: qsTr("Audio language") }
                    Choice { title: qsTr("Stereo mode"); choices: Player.audioStereoMode }
                }
                Section {
                    title: qsTr("Synchronization"); visible: root.currentTab === 2
                    ValueSlider { title: qsTr("Audio track delay"); from: -10; to: 10; step: 0.05; current: Player.audioDelayMS / 1000; suffix: " s"; onEdited: value => { Player.audioDelayMS = Math.round(value * 1000) } }
                }
                Repeater {
                    model: [{key:"equalizer",title:qsTr("Equalizer")},{key:"compressor",title:qsTr("Compressor")},{key:"spatializer",title:qsTr("Spatializer")},{key:"stereo_widen",title:qsTr("Stereo widener")},{key:"normvol",title:qsTr("Volume normalization")},{key:"scaletempo_pitch",title:qsTr("Pitch")},{key:"stereo_pan",title:qsTr("Stereo pan")}]
                    FilterSection { required property var modelData; visible: root.currentTab === 2; filter: modelData.key; title: modelData.title; audio: true }
                }
                Section {
                    title: qsTr("Subtitle Tracks"); visible: root.currentTab === 3
                    Tracks { tracks: Player.subtitleTracks }
                    RowLayout {
                        Action { text: qsTr("Add subtitles"); onClicked: batchSubtitles.open() }
                        Action { text: qsTr("Download"); onClicked: Player.openVLsub() }
                    }
                }
                Section {
                    title: qsTr("Defaults"); visible: root.currentTab === 3
                    OptionRow { key: "sub-track"; title: qsTr("Subtitle track") }
                    OptionRow { key: "sub-language"; title: qsTr("Subtitle language") }
                }
                Section {
                    title: qsTr("Synchronization"); visible: root.currentTab === 3
                    ValueSlider { title: qsTr("Subtitle track delay"); from: -10; to: 10; step: 0.05; current: Player.subtitleDelayMS / 1000; suffix: " s"; onEdited: value => { Player.subtitleDelayMS = Math.round(value * 1000) } }
                    ValueSlider { title: qsTr("Secondary subtitle delay"); from: -10; to: 10; step: 0.05; current: Player.secondarySubtitleDelayMS / 1000; suffix: " s"; onEdited: value => { Player.secondarySubtitleDelayMS = Math.round(value * 1000) } }
                    ValueSlider { title: qsTr("Subtitle speed"); from: 1; to: 100; step: 0.1; current: Player.subtitleFPS; suffix: " fps"; onEdited: value => { Player.subtitleFPS = value } }
                    OptionRow { key: "subsdelay-factor"; title: qsTr("Subtitle duration factor") }
                }
                Section {
                    title: qsTr("Appearance"); visible: root.currentTab === 3
                    RowLayout {
                        Text { text: qsTr("Font size"); color: root.ink; Layout.fillWidth: true; font.pixelSize: 14 * root.unit }
                        Select {
                            Layout.preferredWidth: 170 * root.unit
                            model: [qsTr("Relative"), qsTr("Precise")]
                            currentIndex: Number(root.read("freetype-fontsize")) > 0 ? 1 : 0
                            onActivated: {
                                if (currentIndex === 1) root.write("sub-text-scale", 100)
                                root.write("freetype-fontsize", currentIndex === 0 ? 0 : 32)
                            }
                        }
                    }
                    ValueSlider { visible: Number(root.read("freetype-fontsize")) === 0; title: qsTr("Font scale"); from: 10; to: 500; step: 1; current: Number(root.read("sub-text-scale")); suffix: " %"; onEdited: value => root.write("sub-text-scale", Math.round(value)) }
                    ValueSlider { visible: Number(root.read("freetype-fontsize")) > 0; title: qsTr("Font size"); from: 1; to: 512; step: 1; current: Number(root.read("freetype-fontsize")); suffix: " px"; onEdited: value => root.write("freetype-fontsize", Math.round(value)) }
                    Repeater {
                        model: ["freetype-font","freetype-bold","freetype-color","freetype-opacity","freetype-outline-color","freetype-outline-thickness","freetype-shadow-color","freetype-shadow-distance","freetype-background-opacity","freetype-background-color","sub-margin"]
                        OptionRow { required property var modelData; key: modelData }
                    }
                }
            }
        }
        Loader {
            Layout.fillHeight: true; Layout.fillWidth: true
            visible: root.legacyTools; active: root.legacyTools
            sourceComponent: C.StackView {
                id: nativeTools
                initialItem: TracksListPage { trackMenuController: nativeController }
                QtObject {
                    id: nativeController
                    signal requestAudioPage()
                    signal requestSubtitlePage()
                    signal requestPlaybackSpeedPage()
                    signal requestBack()
                    onRequestAudioPage: nativeTools.push("qrc:///qt/qml/VLC/Player/TracksPageAudio.qml", {trackMenuController:nativeController})
                    onRequestSubtitlePage: nativeTools.push("qrc:///qt/qml/VLC/Player/TracksPageSubtitle.qml", {trackMenuController:nativeController})
                    onRequestPlaybackSpeedPage: nativeTools.push("qrc:///qt/qml/VLC/Player/TracksPageSpeed.qml", {trackMenuController:nativeController})
                    onRequestBack: nativeTools.pop()
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: root.edge }
        ColumnLayout {
            Layout.fillWidth: true; Layout.leftMargin: 24 * root.unit; Layout.rightMargin: 24 * root.unit
            Layout.topMargin: 14 * root.unit; Layout.bottomMargin: 18 * root.unit; spacing: 8 * root.unit
            Select { objectName: "redesignPresetSelect"; openUp: true; Layout.fillWidth: true; model: root.presetNames(); displayText: currentIndex >= 0 ? currentText : qsTr("Default Preset"); onActivated: root.applyPreset(currentText) }
            RowLayout {
                Field { id: presetName; objectName:"redesignPresetName";Layout.fillWidth: true; placeholderText: qsTr("Preset name") }
                Action { text: qsTr("Save"); onClicked: root.savePreset() }
            }
            RowLayout {
                Action { text: root.legacyTools ? qsTr("Back") : qsTr("Track tools"); onClicked: root.legacyTools = !root.legacyTools }
                Action { text: qsTr("All effects"); onClicked: DialogsProvider.extendedDialog() }
                Action { text: qsTr("Settings"); onClicked: DialogsProvider.prefsDialog() }
            }
            Text { visible: root.status.length > 0; text: root.status; color: root.colorContext.accent; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
    }
    RedesignSubtitleImport { id: batchSubtitles; parent: C.Overlay.overlay }
    // Controls below mirror RedesignSettings (Figma): filled fields without outlines, the
    // image chevron, a rounded list with an accent check, and knob-style switches.
    component Field: C.TextField {
        implicitHeight: 34 * root.unit
        color: root.ink; placeholderTextColor: root.muted
        selectionColor: root.colorContext.accent; selectedTextColor: VLCStyle.onAccent(root.colorContext.accent)
        font.pixelSize: 14 * root.unit; leftPadding: 14 * root.unit; rightPadding: 14 * root.unit
        background: Rectangle { color: root.field; radius: 6 * root.unit; border.width: 2; border.color: parent.activeFocus ? root.colorContext.accent : root.panel }
    }
    component Action: C.Button {
        id: action
        implicitHeight: 34 * root.unit
        font.pixelSize: 14 * root.unit; leftPadding: 14 * root.unit; rightPadding: 14 * root.unit
        contentItem: Text { text: action.text; font: action.font; color: action.hovered ? root.title : root.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: action.down ? root.pressed : action.hovered ? root.hover : root.panel; radius: 6 * root.unit; border.width: 2; border.color: action.visualFocus ? root.colorContext.accent : "transparent" }
    }
    component Select: C.ComboBox {
        id: select
        property bool openUp: false
        implicitHeight: 34 * root.unit
        font.pixelSize: 14 * root.unit; leftPadding: 14 * root.unit; rightPadding: 38 * root.unit
        contentItem: Text { text: select.displayText; font: select.font; color: select.enabled ? root.ink : root.muted; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        C.ToolTip.visible: hovered && contentItem.truncated; C.ToolTip.text: displayText; C.ToolTip.delay: 500
        background: Rectangle { color: root.panel; radius: 6 * root.unit; border.width: 2; border.color: select.visualFocus ? root.colorContext.accent : root.field }
        indicator: Image {
            x: select.width - width - 10 * root.unit; y: (select.height - height) / 2
            width: 24 * root.unit; height: width; sourceSize: Qt.size(width, height); opacity: select.enabled ? 1 : .5
            source: VLCStyle.iconUrl("settings-imgFrame.svg", root.ink)
            rotation: select.popup.visible !== select.openUp ? 180 : 0
        }
        popup: C.Popup {
            y: select.openUp ? -height - 4 * root.unit : select.height + 4 * root.unit
            width: select.width; padding: 4 * root.unit
            implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320 * root.unit)
            background: Rectangle { color: root.panel; radius: 8 * root.unit; border.color: root.field; border.width: 2 }
            contentItem: ListView {
                clip: true; implicitHeight: contentHeight
                model: select.popup.visible ? select.delegateModel : null
                currentIndex: select.highlightedIndex
                C.ScrollIndicator.vertical: C.ScrollIndicator {}
            }
        }
        delegate: C.ItemDelegate {
            id: entry
            required property var model
            required property int index
            readonly property bool current: index === select.currentIndex
            width: select.width - 8 * root.unit; height: 36 * root.unit
            leftPadding: 12 * root.unit; rightPadding: 12 * root.unit
            text: model.label ?? model.display ?? model.modelData
            highlighted: select.highlightedIndex === index
            contentItem: RowLayout {
                spacing: 8 * root.unit
                Text { text: entry.text; color: entry.current ? root.title : root.ink; font.pixelSize: 14 * root.unit; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: "✓"; visible: entry.current; color: root.colorContext.accent; font.pixelSize: 16 * root.unit }
            }
            background: Rectangle { color: entry.highlighted || entry.current ? root.hover : "transparent"; radius: 4 * root.unit }
        }
    }
    component Toggle: C.CheckBox {
        id: toggle
        implicitWidth: 68 * root.unit; implicitHeight: 32 * root.unit
        Accessible.name: text
        contentItem: Item {}
        indicator: Rectangle {
            width: 68 * root.unit; height: 32 * root.unit; radius: 6 * root.unit
            color: toggle.checked ? (root.dark ? Qt.darker(root.colorContext.accent, 3.8) : Qt.alpha(root.colorContext.accent, .22)) : root.panel
            border.width: 2; border.color: toggle.visualFocus ? root.colorContext.accent : root.field
            Rectangle { x: toggle.checked ? 36 * root.unit : 3 * root.unit; y: 3 * root.unit; width: 26 * root.unit; height: 26 * root.unit; radius: 3 * root.unit; color: toggle.checked ? Qt.darker(root.colorContext.accent, 1.2) : root.knob }
            Text { x: toggle.checked ? 4 * root.unit : 34 * root.unit; width: 30 * root.unit; height: parent.height; text: toggle.checked ? qsTr("ON") : qsTr("OFF"); font.pixelSize: 12 * root.unit; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                color: toggle.checked ? (root.dark ? root.colorContext.accent : Qt.darker(root.colorContext.accent, 1.4)) : root.muted }
        }
    }
    component Range: C.Slider {
        id: range
        implicitHeight: 22 * root.unit
        background: Rectangle { x: range.leftPadding; y: (range.height-height)/2; width: range.availableWidth; height: 4 * root.unit; radius: 2 * root.unit; color: root.track; Rectangle { width: parent.width * range.visualPosition; height: parent.height; color: root.colorContext.accent; radius: parent.radius } }
        handle: Rectangle { x: range.leftPadding + range.visualPosition * (range.availableWidth-width); y: (range.height-height)/2; width: 18 * root.unit; height: width; radius: width / 2; color: root.colorContext.accent; border.color: root.title; border.width: range.visualFocus ? 1 : 0 }
    }
    component Section: ColumnLayout {
        id: section
        property string title
        default property alias entries: entries.data
        Layout.fillWidth: true; spacing: 12 * root.unit
        Rectangle { Layout.fillWidth: true; height: 1; color: root.edge }
        Text { text: section.title; color: root.title; font.pixelSize: 16 * root.unit; Layout.leftMargin: 24 * root.unit; Layout.topMargin: 12 * root.unit }
        ColumnLayout { id: entries; Layout.fillWidth: true; Layout.margins: 24 * root.unit; spacing: 14 * root.unit }
    }
    component Tracks: ColumnLayout {
        required property var tracks
        Layout.fillWidth: true; spacing: 2
        Repeater {
            model: tracks
            // a selectable row with the dropdown's accent check instead of a stock checkbox
            C.ItemDelegate {
                id: trackRow
                required property var model
                Layout.fillWidth: true; implicitHeight: 40 * root.unit
                leftPadding: 12 * root.unit; rightPadding: 12 * root.unit
                text: model.display; font.pixelSize: 14 * root.unit
                checked: model.checked
                Accessible.role: Accessible.CheckBox; Accessible.checked: checked
                contentItem: RowLayout {
                    spacing: 8 * root.unit
                    Text { text: trackRow.text; color: trackRow.checked ? root.title : root.ink; font: trackRow.font; elide: Text.ElideRight; Layout.fillWidth: true }
                    Text { text: "✓"; visible: trackRow.checked; color: root.colorContext.accent; font.pixelSize: 16 * root.unit }
                }
                background: Rectangle { color: trackRow.down ? root.pressed : trackRow.hovered || trackRow.checked ? root.hover : "transparent"; radius: 6 * root.unit; border.width: 2; border.color: trackRow.visualFocus ? root.colorContext.accent : "transparent" }
                onClicked: model.checked = !model.checked
            }
        }
    }
    component Choice: RowLayout {
        required property string title
        required property var choices
        Layout.fillWidth: true
        Text { text: parent.title; color: root.ink; Layout.fillWidth: true; font.pixelSize: 14 * root.unit }
        Select { Layout.preferredWidth: 170 * root.unit; model: parent.choices; textRole: "display"; currentIndex: parent.choices.currentRow; onActivated: parent.choices.toggleIndex(currentIndex)
            // the active output may not offer this setting (or any device list)
            enabled: count > 0; displayText: count === 0 ? qsTr("Not available") : currentIndex < 0 ? qsTr("Default") : currentText }
    }
    component ValueSlider: ColumnLayout {
        id: valueSlider
        required property string title
        property real from: 0
        property real to: 1
        property real step: 0.01
        property real current: 0
        property string suffix: ""
        signal edited(real value)
        Layout.fillWidth: true; spacing: 10 * root.unit
        RowLayout {
            Text { text: valueSlider.title; color: root.ink; Layout.fillWidth: true; font.pixelSize: 14 * root.unit }
            Field {
                Layout.preferredWidth: 170 * root.unit
                text: valueSlider.current.toFixed(3) + valueSlider.suffix
                horizontalAlignment: Text.AlignRight
                onEditingFinished: { const parsed = parseFloat(text); if (isFinite(parsed)) valueSlider.edited(Math.max(valueSlider.from, Math.min(valueSlider.to, parsed))) }
            }
        }
        Range { Layout.fillWidth: true; from: valueSlider.from; to: valueSlider.to; stepSize: valueSlider.step; value: valueSlider.current; onMoved: valueSlider.edited(value) }
    }
    component OptionRow: RowLayout {
        id:row
        required property string key
        property string title:""
        readonly property var option:root.option(key)
        visible:!!option;Layout.fillWidth:true
        Text {text:row.title||row.option?.label||"";color:root.ink;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:13*root.unit}
        Loader {
            Layout.preferredWidth:170*root.unit
            sourceComponent:row.option?.type==="bool"?boolOption:row.option?.choices?.length>0&&!row.option?.color?choiceOption:textOption
        }
        Component {id:boolOption;Toggle {checked:!!root.read(row.key);onClicked:root.write(row.key,checked)}}
        Component {id:choiceOption;Select {model:row.option.choices;textRole:"label";currentIndex:row.option.choices.findIndex(c=>c.value===root.read(row.key));onActivated:root.write(row.key,row.option.choices[currentIndex].value)}}
        Component {id:textOption;Field {
            // -1 on the *-track options means "pick automatically"
            readonly property bool automatic:/-track$/.test(row.key)&&Number(root.read(row.key))===-1
            placeholderText:automatic?qsTr("Auto"):""
            text:row.option?.color?"#"+("000000"+Number(root.read(row.key)).toString(16)).slice(-6):automatic?"":String(root.read(row.key)??"")
            onEditingFinished:{let value=text;if(row.option?.color)value=parseInt(text.replace("#",""),16);else if(row.option?.type==="int"||row.option?.type==="float")value=text.trim()===""&&/-track$/.test(row.key)?-1:Number(text);root.write(row.key,value)}
        }}
    }
    component FilterSection: Section {
        id: filterSection
        required property string filter
        property bool audio: false
        property bool expanded: false
        RowLayout {
            Action { text: filterSection.expanded ? qsTr("Hide controls") : qsTr("Show controls"); onClicked: filterSection.expanded = !filterSection.expanded }
            Toggle {
                text: qsTr("ON")
                checked: String(root.read(filterSection.audio ? "audio-filter" : "video-filter") ?? "").split(":").includes(filterSection.filter)
                onClicked: { MainCtx.setRedesignFilter(filterSection.filter, checked); root.revision++ }
            }
        }
        Flow {
            visible:filterSection.filter==="equalizer"||filterSection.filter==="adjust";Layout.fillWidth:true;spacing:6*root.unit
            Repeater {
                model:filterSection.filter==="equalizer"?[{name:qsTr("Balanced"),preset:"flat",bars:[.5,.5,.5]},{name:qsTr("Bass"),preset:"fullbass",bars:[.9,.5,.3]},{name:qsTr("Rock"),preset:"rock",bars:[.8,.3,.8]},{name:qsTr("Voice"),preset:"pop",bars:[.3,.8,.4]}]:[{name:qsTr("Original"),values:{brightness:1,contrast:1,saturation:1,gamma:1,hue:0}},{name:qsTr("Vivid"),values:{brightness:1,contrast:1.15,saturation:1.2,gamma:1,hue:0}},{name:qsTr("Monochrome"),values:{brightness:1,contrast:1,saturation:0,gamma:1,hue:0}}]
                delegate:Action {
                    required property var modelData
                    text:modelData.name;width:88*root.unit;height:68*root.unit
                    contentItem:Column {
                        spacing:4
                        Row {visible:!!parent.parent.modelData.bars;anchors.horizontalCenter:parent.horizontalCenter;spacing:3;height:24*root.unit;Repeater {model:parent.parent.parent.modelData.bars||[];Rectangle {required property real modelData;width:8*root.unit;height:20*root.unit*modelData;anchors.bottom:parent.bottom;color:root.colorContext.accent}}}
                        Row {visible:!parent.parent.modelData.bars;anchors.horizontalCenter:parent.horizontalCenter;height:24*root.unit;spacing:1
                            Repeater {model:parent.parent.parent.modelData.name===qsTr("Monochrome")?["#444444","#888888","#cccccc"]:parent.parent.parent.modelData.name===qsTr("Vivid")?["#ed7640","#23d07b","#39aaff"]:["#976346","#648674","#5e7e97"];Rectangle {required property string modelData;width:14*root.unit;height:24*root.unit;color:modelData;radius:2}}
                        }
                        Text {width:parent.width;text:parent.parent.modelData.name;color:root.ink;horizontalAlignment:Text.AlignHCenter;font.pixelSize:11*root.unit}
                    }
                    onClicked:{MainCtx.setRedesignFilter(filterSection.filter,true);if(modelData.preset)root.write("equalizer-preset",modelData.preset);else {MainCtx.applyRedesignLiveOptions(modelData.values);root.revision++}}
                }
            }
        }
        ColumnLayout {
            visible: filterSection.expanded && filterSection.filter === "equalizer"
            Layout.fillWidth: true
            OptionRow { key: "equalizer-preset"; title: qsTr("Preset") }
            ValueSlider { title: qsTr("Preamp"); from: -20; to: 20; step: .1; current: Number(root.read("equalizer-preamp") ?? 0); suffix: " dB"; onEdited: value => root.write("equalizer-preamp",value) }
            RowLayout {
                Layout.fillWidth: true; spacing: 2 * root.unit
                Repeater {
                    model: root.read("equalizer-vlcfreqs") ? [60,170,310,600,1000,3000,6000,12000,14000,16000] : [31.25,62.5,125,250,500,1000,2000,4000,8000,16000]
                    ColumnLayout {
                        required property real modelData
                        required property int index
                        Layout.fillWidth: true
                        Text { text: root.equalizerBands()[parent.index].toFixed(1); color: root.ink; font.pixelSize: 9 * root.unit; Layout.alignment: Qt.AlignHCenter }
                        C.Slider {
                            orientation: Qt.Vertical; from: -20; to: 20; stepSize: .1
                            value: root.equalizerBands()[parent.index]; Layout.preferredHeight: 120 * root.unit; Layout.alignment: Qt.AlignHCenter
                            onMoved: root.setEqualizerBand(parent.index,value)
                            background: Rectangle { x: (parent.width-width)/2; y: parent.topPadding; width: 3 * root.unit; height: parent.availableHeight; radius: width/2; color: root.track
                                Rectangle { width: parent.width; radius: parent.radius; color: root.colorContext.accent; opacity: .55; y: Math.min(parent.height / 2, parent.parent.visualPosition * parent.height); height: Math.abs(parent.parent.visualPosition - .5) * parent.height }
                            }
                            handle: Rectangle { x: (parent.width-width)/2; y: parent.topPadding + parent.visualPosition * (parent.availableHeight-height); width: 12 * root.unit; height: 7 * root.unit; radius: 2 * root.unit; color: root.colorContext.accent }
                        }
                        Text { text: parent.modelData >= 1000 ? parent.modelData/1000 + "k" : parent.modelData; color: root.ink; font.pixelSize: 9 * root.unit; Layout.alignment: Qt.AlignHCenter }
                    }
                }
            }
            OptionRow { key: "equalizer-2pass"; title: qsTr("Two pass") }
            OptionRow { key: "equalizer-vlcfreqs"; title: qsTr("VLC frequency bands") }
        }
        Repeater { model: filterSection.expanded && filterSection.filter !== "equalizer" ? root.options.filter(option => option.module === filterSection.filter) : []; OptionRow { required property var modelData; key: modelData.key } }
    }
}
