// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as C
import VLC.MainInterface
import VLC.Player
import VLC.Style

C.Popup {
    id: root
    objectName: "redesignSubtitleImport"
    width: Math.min(1048 * VLCStyle.scale, parent.width - 32)
    height: Math.min(720 * VLCStyle.scale, parent.height - 32)
    x: (parent.width-width)/2; y: (parent.height-height)/2
    modal: true; focus: true; padding: 24 * VLCStyle.scale
    property string folder: ""
    property var rows: []
    property var searchRows: []
    property bool hasSearched: false
    property string message: ""
    property int revision: 0
    property int mode: 0
    property bool showAccount: false
    readonly property var provider: MainCtx.redesignSubtitleDownloadState
    readonly property ColorContext colorContext: ColorContext { palette: VLCStyle.palette; colorSet: ColorContext.Window }
    readonly property color paper: colorContext.palette.isDark ? "#0e0e0e" : "#f8f8f8"
    readonly property color ink: colorContext.palette.isDark ? "#8f8f8f" : "#333333"
    palette.text: ink; palette.buttonText: ink; palette.base: colorContext.palette.isDark ? "#090909" : "white"
    palette.window: paper; palette.highlight: colorContext.accent
    background: Rectangle { color: root.paper; radius: 14 * VLCStyle.scale; border.color: root.colorContext.palette.isDark ? "#373737" : "#cccccc" }
    function scope() { return ["current", "folder", "playlist"][source.currentIndex] }
    function updateRow(index, changes) { let rows = root.rows.slice(); rows[index] = Object.assign({},rows[index],changes); root.rows = rows }
    function updateSearchRow(index,changes) { let next=searchRows.slice(); next[index]=Object.assign({},next[index],changes); searchRows=next }
    function refresh() {
        MainCtx.cancelRedesignSubtitleDownloads()
        rows = MainCtx.previewRedesignSubtitles(scope(), folder)
        searchRows = rows.map(row=>Object.assign({},row,{enabled:true,query:row.label.replace(/\.[^.]+$/,""),candidates:[],selected:-1}))
        hasSearched = false; message = ""; revision++
    }
    function apply() {
        const pairs = rows.filter(row => row.enabled && row.selected >= 0).map(row => ({media:row.media, subtitle:row.candidates[row.selected].url, enabled:true}))
        if (!pairs.length) { message = qsTr("Choose at least one subtitle file."); return }
        if (MainCtx.saveRedesignSubtitlePairs(pairs)) close()
        else message = qsTr("A file is unavailable. Refresh the matches and try again.")
    }
    onOpened: refresh()
    onClosed: { password.clear(); MainCtx.cancelRedesignSubtitleDownloads() }
    contentItem: ColumnLayout {
        spacing: 18 * VLCStyle.scale
        RowLayout {
            Text { text: qsTr("Add subtitles"); color: root.ink; font.pixelSize: 20 * VLCStyle.scale; Layout.fillWidth: true }
            C.ToolButton { text: "×"; onClicked: root.close(); Accessible.name: qsTr("Close") }
        }
        C.TabBar {
            Layout.fillWidth: true; currentIndex: root.mode
            onCurrentIndexChanged: root.mode = currentIndex
            background: Rectangle { color: root.paper }
            Repeater {
                model: [qsTr("Local files"), qsTr("Download subtitles")]
                C.TabButton {
                    required property string modelData
                    required property int index
                    text: modelData; width: 180 * VLCStyle.scale
                    background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: root.colorContext.accent; visible: root.mode === index } }
                }
            }
        }
        RowLayout {
            Text { text: qsTr("Add to"); color: root.ink }
            C.ComboBox { id: source; enabled: !root.provider.busy; model: [qsTr("Current file"), qsTr("Folder"), qsTr("Playlist")]; Layout.preferredWidth: 160 * VLCStyle.scale; onActivated: root.refresh() }
            C.TextField { id: directory; text: root.folder; placeholderText: qsTr("Subtitle folder (optional)"); Layout.fillWidth: true; onEditingFinished: { root.folder = text; root.refresh() } }
            C.Button { text: qsTr("Browse"); onClicked: { const folder = MainCtx.pickRedesignPath("redesign-media-folder",root.folder); if (folder) { root.folder = folder; root.refresh() } } }
        }
        Text { text: root.mode === 0 ? qsTr("Review each match. Multiple candidates require a choice. Files stay in their original location.") : qsTr("OpenSubtitles.com · Choose a language, search, then review each result. Downloads use your account allowance."); color: root.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true; textFormat: Text.PlainText }
        RowLayout {
            visible: root.mode === 1; Layout.fillWidth: true
            Text { text: qsTr("Language"); color: root.ink }
            C.ComboBox {
                id: language; Layout.preferredWidth: 160 * VLCStyle.scale; enabled: !root.provider.busy
                model: [{label:qsTr("English"),code:"en"},{label:qsTr("German"),code:"de"},{label:qsTr("Spanish"),code:"es"},{label:qsTr("French"),code:"fr"},{label:qsTr("Italian"),code:"it"},{label:qsTr("Portuguese"),code:"pt"},{label:qsTr("Brazilian Portuguese"),code:"pt-br"},{label:qsTr("Japanese"),code:"ja"},{label:qsTr("Korean"),code:"ko"},{label:qsTr("Chinese"),code:"zh-cn"},{label:qsTr("Arabic"),code:"ar"},{label:qsTr("Turkish"),code:"tr"},{label:qsTr("Dutch"),code:"nl"},{label:qsTr("Polish"),code:"pl"},{label:qsTr("Swedish"),code:"sv"},{label:qsTr("Russian"),code:"ru"}]
                textRole: "label"
            }
            C.Button { text: qsTr("Search"); enabled: !root.provider.busy; onClicked: { MainCtx.searchRedesignSubtitles(root.searchRows,language.model[language.currentIndex].code); root.hasSearched=true } }
            Item { Layout.fillWidth: true }
            C.Button { text: root.provider.authenticated ? qsTr("Account") : qsTr("Sign in"); enabled: !root.provider.busy; onClicked: root.showAccount = !root.showAccount }
        }
        ColumnLayout {
            visible: root.mode === 1 && root.showAccount; Layout.fillWidth: true
            Text { text: root.provider.authenticated ? qsTr("Signed in for this VLC session.") : qsTr("OpenSubtitles.com account"); color: root.ink }
            RowLayout {
                visible: !root.provider.authenticated; Layout.fillWidth: true
                C.TextField { id: username; placeholderText: qsTr("Username"); Layout.fillWidth: true; enabled: !root.provider.busy }
                C.TextField { id: password; placeholderText: qsTr("Password"); echoMode: TextInput.Password; Layout.fillWidth: true; enabled: !root.provider.busy }
                C.Button { text: qsTr("Sign in"); enabled: !root.provider.busy; onClicked: { MainCtx.loginRedesignSubtitles(username.text,password.text,applicationKey.text); password.clear() } }
            }
            C.TextField { id: applicationKey; visible: !root.provider.authenticated; placeholderText: qsTr("Custom application key (optional)"); echoMode: TextInput.Password; Layout.fillWidth: true; enabled: !root.provider.busy }
            RowLayout {
                C.Button { visible: !root.provider.authenticated; text: qsTr("Create an account"); onClicked: Qt.openUrlExternally("https://www.opensubtitles.com/en/users/sign_up") }
                C.Button { visible: root.provider.authenticated; text: qsTr("Sign out"); onClicked: MainCtx.logoutRedesignSubtitles() }
                Text { text: qsTr("Your password and session token are not saved."); color: root.ink; font.pixelSize: 12 * VLCStyle.scale; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        C.ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 8 * VLCStyle.scale
                Repeater {
                    model: root.mode === 0 ? root.rows : root.hasSearched ? (root.provider.rows || []) : root.searchRows
                    RowLayout {
                        id: row
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true; spacing: 12 * VLCStyle.scale
                        C.CheckBox { enabled: !root.provider.busy; checked: row.modelData.enabled; onClicked: { if (root.mode === 0) root.updateRow(row.index,{enabled:checked}); else if (!root.hasSearched) root.updateSearchRow(row.index,{enabled:checked}); else MainCtx.selectRedesignSubtitleDownload(row.index,row.modelData.selected,checked) } }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: 140 * VLCStyle.scale
                            Text { text: row.modelData.label; color: root.ink; elide: Text.ElideMiddle; Layout.fillWidth: true; textFormat: Text.PlainText }
                            Text { visible: root.mode === 1; text: row.modelData.status || ""; color: root.ink; font.pixelSize: 11 * VLCStyle.scale; textFormat: Text.PlainText }
                            C.TextField { visible: root.mode === 1 && !root.hasSearched; text: row.modelData.query || ""; placeholderText: qsTr("Search title"); Layout.fillWidth: true; onEditingFinished: root.updateSearchRow(row.index,{query:text}) }
                        }
                        C.ComboBox {
                            visible: root.mode === 0 || root.hasSearched
                            Layout.preferredWidth: 250 * VLCStyle.scale
                            model: row.modelData.candidates; textRole: "label"; currentIndex: row.modelData.selected
                            displayText: currentIndex < 0 ? qsTr("Choose subtitles") : currentText
                            enabled: !root.provider.busy
                            onActivated: { if (root.mode === 0) root.updateRow(row.index,{selected:currentIndex,enabled:true}); else MainCtx.selectRedesignSubtitleDownload(row.index,currentIndex,true) }
                        }
                        C.Button {
                            text: qsTr("Choose file")
                            visible: root.mode === 0
                            onClicked: {
                                const file = MainCtx.pickRedesignPath("redesign-subtitle-file",root.folder)
                                if (!file) return
                                let rows = root.rows.slice(); let updated = Object.assign({},row.modelData)
                                const url = MainCtx.redesignLocalFileUrl(file)
                                updated.candidates = updated.candidates.concat([{label:file.split(/[\\/]/).pop(),url:url}]); updated.selected = updated.candidates.length-1; updated.enabled = true
                                rows[row.index] = updated; root.rows = rows
                            }
                        }
                    }
                }
                Text { visible: root.mode === 0 ? root.rows.length === 0 : root.hasSearched ? !(root.provider.rows || []).length : root.searchRows.length===0; text: root.mode === 1 ? qsTr("Choose the scope above and search for subtitles.") : source.currentIndex === 1 && !root.folder ? qsTr("Choose the folder containing your media files.") : qsTr("No media files in this selection."); color: root.ink; Layout.fillWidth: true }
            }
        }
        Text { visible: root.message.length > 0; text: root.message; color: root.colorContext.accent; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { visible: root.mode === 1 && (root.provider.message || "").length > 0; text: root.provider.message || ""; color: root.ink; Layout.fillWidth: true; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        C.ProgressBar { visible: root.mode === 1 && root.provider.busy; Layout.fillWidth: true; from: 0; to: Math.max(1,root.provider.total || 1); value: root.provider.completed || 0; indeterminate: !(root.provider.total || 0) }
        RowLayout {
            C.Button { visible: root.mode === 1; text: root.provider.busy ? qsTr("Stop") : qsTr("Download selected"); onClicked: { if (root.provider.busy) MainCtx.cancelRedesignSubtitleDownloads(); else { if (!root.provider.authenticated) root.showAccount = true; MainCtx.downloadRedesignSubtitles() } } }
            Item { Layout.fillWidth: true }
            C.Button { text: qsTr("Cancel"); onClicked: root.close() }
            C.Button { text: qsTr("Apply"); enabled: !root.provider.busy; onClicked: { if (root.mode === 0) root.apply(); else if (MainCtx.applyRedesignSubtitleDownloads()) root.close(); else root.message = qsTr("Download at least one selected result before applying it.") } }
        }
    }
}
