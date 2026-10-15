// SPDX-License-Identifier: GPL-2.0-or-later
// Compact library from VLC's existing asynchronous media-library models.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as C
import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Player
import VLC.Playlist
import VLC.Dialogs
import VLC.Style
import VLC.Widgets as Widgets

C.Popup {
    id: root
    objectName: "redesignLibraryPanel"
    property int currentTab: 0
    property int section: 0
    readonly property var activeModel: currentTab === 0 ? videos : currentTab === 1 ? music : currentTab === 3 ? playlists : streams
    readonly property ColorContext colorContext: ColorContext { colorSet: ColorContext.View; palette: VLCStyle.palette }
    readonly property color paper: (colorContext.palette?.isDark ?? true) ? "#0e0e0e" : "#f8f8f8"
    readonly property color panel: (colorContext.palette?.isDark ?? true) ? "#191919" : "#eaeaea"
    readonly property color field: (colorContext.palette?.isDark ?? true) ? "#090909" : "#ffffff"
    readonly property color edge: (colorContext.palette?.isDark ?? true) ? "#373737" : "#cccccc"
    width: Math.min(400 * VLCStyle.scale, parent.width - 24)
    height: Math.min(945 * VLCStyle.scale, parent.height - 90 * VLCStyle.scale)
    padding: 12 * VLCStyle.scale
    focus: true; modal: false
    closePolicy: C.Popup.CloseOnEscape
    onOpened: { MainCtx.redesignSidebarVisible = true; bookmarks.reload() }
    onClosed: MainCtx.redesignSidebarVisible = false
    palette.text: colorContext.fg.secondary; palette.buttonText: colorContext.fg.secondary
    palette.base: colorContext.bg.primary; palette.button: colorContext.bg.secondary
    palette.highlight: colorContext.accent
    background: Rectangle { color: root.paper; radius: 12 * VLCStyle.scale; border.color: root.edge }
    MLVideoModel { id: videos; ml: MediaLib; searchPattern: search.text; sortCriteria: "title" }
    MLAudioModel { id: music; ml: MediaLib; searchPattern: search.text; sortCriteria: "title" }
    MLMediaModel { id: streams; ml: MediaLib; searchPattern: search.text; sortCriteria: "title" }
    MLPlaylistListModel { id: playlists; ml: MediaLib; searchPattern: search.text; playlistType: MLPlaylistListModel.PLAYLIST_TYPE_ALL }
    MLBookmarkModel { id: bookmarks; objectName: "redesignSidebarBookmarksModel"; ml: MediaLib; player: Player }
    Connections { target: MainCtx; function onRedesignBookmarkAdded(milliseconds,success) { if (success) bookmarks.reload() } }
    contentItem: ColumnLayout {
        readonly property ColorContext colorContext: ColorContext { colorSet: ColorContext.View; palette: root.colorContext.palette }
        spacing: 12 * VLCStyle.scale
        RowLayout {
            Layout.fillWidth: true
            Text { text: [qsTr("Library"),qsTr("Play Queue"),qsTr("Bookmarks"),qsTr("Chapters")][root.section]; color: root.colorContext.fg.primary; font.family: "Inter"; font.pixelSize: 16 * VLCStyle.scale; Layout.fillWidth: true }
            Widgets.IconToolButton { text: VLCIcons.close; description: qsTr("Close library"); colorContext.palette: root.colorContext.palette; onClicked: root.close() }
        }
        C.TabBar {
            objectName: "redesignSidebarModes"
            Layout.fillWidth: true
            // the default TabBar background showed through the gaps as bright dividers
            background: Rectangle { color: root.field }
            currentIndex: root.section
            onCurrentIndexChanged: root.section = currentIndex
            Repeater {
                model: [qsTr("Library"),qsTr("Queue"),qsTr("Bookmarks"),qsTr("Chapters")]
                C.TabButton {
                    id: modeTab
                    required property int index
                    required property string modelData
                    objectName: "redesignSidebarSection-" + index
                    text: modelData; font.family: "Inter"; font.pixelSize: 11 * VLCStyle.scale
                    onClicked: root.section = index
                    contentItem: Text {text:modeTab.text;font:modeTab.font;color:modeTab.checked?root.colorContext.accent:root.colorContext.fg.secondary;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                    background: Rectangle {color:root.field;Rectangle {anchors {bottom:parent.bottom;left:parent.left;right:parent.right}height:2;color:modeTab.checked?root.colorContext.accent:"transparent"}}
                    DropArea {anchors.fill:parent;onEntered:(drag)=>{if(modeTab.index===1){root.section=1;drag.accepted=true}}}
                }
            }
        }
        C.TabBar {
            visible: root.section === 0
            Layout.fillWidth: true
            background: Rectangle { color: root.field }
            currentIndex: root.currentTab; onCurrentIndexChanged: root.currentTab = currentIndex
            Repeater {
                model: [qsTr("Videos"), qsTr("Music"), qsTr("Network"), qsTr("Playlists")]
                C.TabButton {
                    id: tab
                    required property string modelData; text: modelData
                    font.family: "Inter"; font.pixelSize: 12 * VLCStyle.scale
                    contentItem: Text { text: tab.text; color: tab.checked ? root.colorContext.accent : root.colorContext.fg.secondary; font: tab.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: root.field; Rectangle { anchors { bottom: parent.bottom; left: parent.left; right: parent.right } height: 2; color: tab.checked ? root.colorContext.accent : "transparent" } }
                }
            }
        }
        C.TextField { id: search; visible:root.section===0; Layout.fillWidth: true; placeholderText: qsTr("Search"); selectByMouse: true; font.family: "Inter"; font.pixelSize: 14 * VLCStyle.scale; color: root.colorContext.fg.secondary; background: Rectangle { color: root.field; border.width: 2; border.color: search.activeFocus ? root.colorContext.accent : ((root.colorContext.palette?.isDark ?? true) ? "#191919" : "#eaeaea"); radius: 6 * VLCStyle.scale } }
        RowLayout {
            visible: root.section === 0 && root.currentTab === 2; Layout.fillWidth: true
            C.TextField { id: streamUrl; Layout.fillWidth: true; placeholderText: qsTr("Stream URL"); selectByMouse: true }
            C.Button { text: qsTr("Open"); enabled: /^(https?|rtsp|rtmp|udp|rtp|srt|mms):/i.test(streamUrl.text.trim()); onClicked: { MainPlaylistController.append([streamUrl.text.trim()], true); root.close() } }
        }
        C.Button {
            visible: root.section === 0 && root.currentTab === 3; Layout.fillWidth: true
            text: qsTr("Create playlist")
            onClicked: { const reply = DialogsProvider.getTextDialog(null, qsTr("Playlist"), qsTr("Playlist name"), ""); if (reply.ok && reply.text.trim()) playlists.create(reply.text.trim(), []) }
        }
        ListView {
            id: entries
            objectName: "redesignLibraryEntries"
            visible: root.section === 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; reuseItems: true
            model: root.activeModel
            C.ScrollBar.vertical: C.ScrollBar { }
            delegate: C.ItemDelegate {
                id: entry
                required property var model
                required property int index
                readonly property bool included: root.currentTab !== 2 || !model.isLocal
                width: entries.width; height: included ? 52 * VLCStyle.scale : 0; visible: included
                text: String(model.title || model.name || model.fileName || "")
                contentItem: RowLayout {
                    spacing: 10 * VLCStyle.scale
                    Image { Layout.preferredWidth: 32 * VLCStyle.scale; Layout.preferredHeight: 32 * VLCStyle.scale; source: entry.model.small_cover || entry.model.thumbnail || entry.model.cover || VLCStyle.noArtVideoCover; fillMode: Image.PreserveAspectCrop }
                    Text { text: entry.text; textFormat: Text.PlainText; elide: Text.ElideRight; Layout.fillWidth: true; color: root.colorContext.fg.secondary; font.family: "Inter"; font.pixelSize: 14 * VLCStyle.scale }
                    Widgets.IconToolButton {
                        text: "redesign-add-bookmark"
                        description: qsTr("Pin to sidebar")
                        colorContext.palette: root.colorContext.palette
                        onClicked: {
                            const kind = root.currentTab === 3 ? "playlist" : "media"
                            const target = root.currentTab === 3 ? entry.model.id.toString() : String(entry.model.mrl || "")
                            if (VLCStyle.pinnedItems.some(pin => pin.kind === kind && pin.target === target)) VLCStyle.unpinItem(kind, target)
                            else VLCStyle.pinItem(kind, target, entry.text)
                        }
                    }
                    Widgets.IconToolButton {
                        objectName: "redesignLibraryQueue-" + entry.index
                        text: VLCIcons.playlist; description: qsTr("Add to play queue")
                        colorContext.palette: root.colorContext.palette
                        onClicked: MediaLib.addToPlaylist(entry.model.id)
                    }
                }
                onClicked: { MediaLib.addAndPlay(model.id); root.close() }
                Drag.active: drag.active
                Drag.dragType: Drag.Automatic
                Drag.supportedActions: Qt.CopyAction
                Drag.mimeData: ({"text/uri-list": String(model.mrl || "")})
                DragHandler { id: drag; target: null; enabled: !!entry.model.mrl }
            }
        }
        RowLayout {
            visible: root.section === 0
            Layout.fillWidth: true
            C.Button {
                text: qsTr("Add folder"); Layout.fillWidth: true
                onClicked: { const folder = MainCtx.pickRedesignPath("redesign-media-folder", ""); if (folder) MainCtx.addRedesignMediaFolder(folder) }
            }
            C.Button { text: qsTr("Full library"); Layout.fillWidth: true; onClicked: { root.close(); MainCtx.playerView = false; History.push([root.currentTab === 0 ? "video" : root.currentTab === 1 ? "music" : root.currentTab === 3 ? "playlists" : "network"]) } }
        }
        Loader {
            visible:root.section===1
            active:visible && root.opened
            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumWidth:0
            sourceComponent:PlaylistPane {
                objectName: "redesignQueuePane"
                useAcrylic:false
                showTitle:false
                background:Rectangle {color:root.paper}
            }
        }
        ListView {
            id: bookmarkEntries
            objectName:"redesignSidebarBookmarkEntries"
            visible:root.section===2
            Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            model:bookmarks
            C.ScrollBar.vertical:C.ScrollBar {}
            delegate:RowLayout {
                required property var model
                required property int index
                width:bookmarkEntries.width;height:48*VLCStyle.scale
                C.Button {
                    objectName:"redesignSidebarBookmarkSeek-"+parent.index
                    text:String(parent.model.time || "")
                    Layout.preferredWidth:116*VLCStyle.scale
                    onClicked:bookmarks.select(bookmarks.index(parent.index,0))
                }
                C.TextField {
                    objectName:"redesignSidebarBookmarkName-"+parent.index
                    text:parent.model.name||"";selectByMouse:true;Layout.fillWidth:true
                    Accessible.name:qsTr("Bookmark name")
                    onEditingFinished:bookmarks.renameAt(parent.index,text)
                }
                Widgets.IconToolButton {
                    objectName:"redesignSidebarBookmarkRemove-"+parent.index
                    text:VLCIcons.close;description:qsTr("Remove bookmark")
                    onClicked:bookmarks.removeAt(parent.index)
                }
            }
            footer:Text {visible:bookmarkEntries.count===0;width:bookmarkEntries.width;text:qsTr("Press Ctrl+B to bookmark the current moment.");wrapMode:Text.WordWrap;color:root.colorContext.fg.secondary;font.pixelSize:14*VLCStyle.scale}
        }
        ListView {
            id:chapterEntries
            objectName:"redesignSidebarChapterEntries"
            visible:root.section===3
            Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            model:Player.chapters
            C.ScrollBar.vertical:C.ScrollBar {}
            delegate:C.ItemDelegate {
                required property var model
                required property int index
                objectName:"redesignSidebarChapterSeek-"+index
                width:chapterEntries.width
                text:String(model.display||model.name||qsTr("Chapter %1").arg(index+1))
                onClicked:Player.chapters.selectChapter(index)
            }
            footer:Text {visible:chapterEntries.count===0;width:chapterEntries.width;text:qsTr("This file has no chapters.");wrapMode:Text.WordWrap;color:root.colorContext.fg.secondary;font.pixelSize:14*VLCStyle.scale}
        }
        RowLayout {
            visible:root.section===2;Layout.fillWidth:true
            C.Button {objectName:"redesignSidebarAddBookmark";text:qsTr("Add bookmark");Layout.fillWidth:true;onClicked:MainCtx.addRedesignBookmark()}
            C.Button {text:qsTr("Advanced editor");onClicked:DialogsProvider.classicBookmarksDialog()}
        }
    }
}
