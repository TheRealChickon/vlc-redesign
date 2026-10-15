// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls.Basic as C
import VLC.MainInterface
import VLC.Player
import VLC.Playlist
import VLC.Style
import VLC.Util

C.ToolButton {
    id: root
    objectName: "redesignControl-" + controlKind
    property bool paintOnly: false
    property string controlKind: ""
    property string controlLabel: ""
    readonly property ColorContext colorContext: ColorContext { colorSet: ColorContext.ToolButton }
    readonly property var location: { const current = Player.url, metadata = Player.title; return MainCtx.redesignMediaLocation() }
    readonly property string value: controlKind === "artist" ? Player.artist
        : controlKind === "folder" ? String(location.folder || "")
        : controlKind === "playlist" ? String(location.playlist || qsTr("Play queue · %1 items").arg(MainPlaylistController.count))
        : String(location.file || Player.name || Player.title || "")
    text: paintOnly ? controlLabel : value
    implicitWidth: controlKind === "thumbnail" ? 36 * VLCStyle.scale : Math.min(200 * VLCStyle.scale, label.implicitWidth + 16 * VLCStyle.scale)
    implicitHeight: 32 * VLCStyle.scale
    enabled: !paintOnly && Player.isStarted
    Accessible.name: controlLabel + (text ? ": " + text : "")
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)
    onClicked: MainCtx.playerView = !MainCtx.playerView
    C.ToolTip.visible: hovered || visualFocus
    C.ToolTip.text: Accessible.name
    C.ToolTip.delay: VLCStyle.delayToolTipAppear
    background: Rectangle { color: root.hovered ? root.colorContext.bg.secondary : "transparent"; radius: 4; border.color: root.visualFocus ? root.colorContext.accent : "transparent" }
    contentItem: Item {
        Text {
            id: label
            anchors.fill: parent
            visible: root.controlKind !== "thumbnail"
            text: root.text
            textFormat: Text.PlainText
            font.family: "Inter"; font.pixelSize: 14 * VLCStyle.scale
            color: root.colorContext.fg.primary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        Image {
            anchors.fill: parent
            visible: root.controlKind === "thumbnail"
            source: Player.artwork.toString() ? Player.artwork : VLCStyle.noArtAlbumCover
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(width, height)
        }
    }
}
