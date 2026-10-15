// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQml
import QtQuick.Controls.Basic as C
import VLC.MainInterface
import VLC.Player
import VLC.Playlist
import VLC.Dialogs
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util

Widgets.IconToolButton {
    id: root
    objectName: "redesignControl-" + controlKind
    property string controlKind: ""
    property string controlLabel: ""
    property int revision: 0
    property string feedbackMessage: ""
    signal requestLockUnlockAutoHide(bool lock)
    description: controlLabel
    text: controlKind === "settings" ? VLCIcons.settings
        : controlKind === "home" ? VLCIcons.home
        : controlKind === "playlists" ? VLCIcons.playlist
        : controlKind === "hdr" ? "HDR"
        : controlKind === "bookmark" ? "redesign-add-bookmark" : "redesign-" + controlKind
    font.family: controlKind === "hdr" ? "Inter" : VLCIcons.fontFamily
    font.pixelSize: controlKind === "hdr" ? VLCStyle.dp(12, VLCStyle.scale) : VLCStyle.icon_toolbar
    enabled: !paintOnly && (controlKind === "pip" ? Player.hasVideoOutput && MainCtx.canShowVideoPIP
        : controlKind === "zoom" ? Player.hasVideoOutput
        : controlKind === "replay" ? Player.seekable
        : controlKind === "bookmark" ? MainCtx.mediaLibraryAvailable && Player.time.valid()
        : controlKind === "favorite" ? MainCtx.redesignFavoriteAvailable && !MainCtx.redesignFavoriteBusy
        : true)
    checked: controlKind === "favorite" && MainCtx.redesignIsFavorite
    C.ToolTip.visible: hovered || visualFocus || feedbackTimer.running
    C.ToolTip.text: feedbackMessage || description
    Timer { id: feedbackTimer; interval: 1800; onTriggered: root.feedbackMessage = "" }
    Connections {
        target: MainCtx
        function onRedesignBookmarkAdded(milliseconds, success) {
            if (root.controlKind !== "bookmark") return
            root.feedbackMessage = success ? qsTr("Bookmark saved") : qsTr("VLC could not save this bookmark")
            feedbackTimer.restart()
        }
    }
    Component.onCompleted: if (controlKind === "favorite" || controlKind === "bookmark") MainCtx.refreshRedesignFavorite()
    onControlKindChanged: if (!paintOnly && (controlKind === "favorite" || controlKind === "bookmark")) MainCtx.refreshRedesignFavorite()
    onClicked: {
        if (controlKind === "pip") MainCtx.setRedesignPopout(!MainCtx.redesignPopoutActive)
        else if (controlKind === "sidebar") MainCtx.askShowRedesignFolder()
        else if (controlKind === "settings") DialogsProvider.prefsDialog()
        else if (controlKind === "home" || controlKind === "playlists") {
            MainCtx.playerView = false
            History.push([controlKind === "home" ? "home" : "playlists"])
        } else if (controlKind === "replay") { Player.jumpToPos(0); MainPlaylistController.play() }
        else if (controlKind === "favorite") MainCtx.toggleRedesignFavorite()
        else if (controlKind === "bookmark") MainCtx.addRedesignBookmark()
        else if (controlKind === "zoom") zoomMenu.popup()
        else if (controlKind === "hdr") { revision++; hdrMenu.popup() }
    }
    C.Menu {
        id: zoomMenu
        onOpened: root.requestLockUnlockAutoHide(true)
        onClosed: root.requestLockUnlockAutoHide(false)
        Instantiator {
            model: Player.zoom
            delegate: C.MenuItem {
                required property int index
                required property var model
                text: model.display
                checked: model.checked
                checkable: true
                onTriggered: Player.zoom.toggleIndex(index)
            }
            onObjectAdded: (index, object) => zoomMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => zoomMenu.removeItem(object)
        }
    }
    C.Menu {
        id: hdrMenu
        onOpened: root.requestLockUnlockAutoHide(true)
        onClosed: root.requestLockUnlockAutoHide(false)
        C.MenuItem { text: qsTr("Applies when video output next starts"); enabled: false }
        Instantiator {
            model: [{value: "auto", label: qsTr("Automatic")}, {value: "never", label: qsTr("SDR output")},
                {value: "always", label: qsTr("HDR output")}, {value: "generate", label: qsTr("Generate HDR from SDR")}]
            delegate: C.MenuItem {
                required property var modelData
                text: modelData.label
                checkable: true
                checked: { const updated = root.revision; return MainCtx.redesignConfigValue("d3d11-hdr-mode") === modelData.value }
                onTriggered: { const values = {}; values["d3d11-hdr-mode"] = modelData.value; MainCtx.saveRedesignConfig(values); root.revision++ }
            }
            onObjectAdded: (index, object) => hdrMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => hdrMenu.removeItem(object)
        }
    }
    function forceUnlock() { zoomMenu.close(); hdrMenu.close() }
}
