/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player
import VLC.Util

T.Control {
    id: root
    width: Math.round(VLCStyle.dp(384, VLCStyle.scale))
    height: width * 9 / 16 + footerHeight
    readonly property real footerHeight: 112 * VLCStyle.scale
    readonly property var location: { const current = Player.url, title = Player.title; return MainCtx.redesignMediaLocation() }
    readonly property ColorContext colorContext: ColorContext { colorSet: ColorContext.View; palette: VLCStyle.palette }

    objectName: "pip window"

    property real dragXMin: 0
    property real dragXMax: 0
    property real dragYMin: undefined
    property real dragYMax: undefined

    MouseArea {
        id:resizeGrip;objectName:"redesignMiniPlayerResize"
        anchors.right:parent.right;anchors.bottom:parent.bottom;width:18*VLCStyle.scale;height:width;z:5
        cursorShape:Qt.SizeFDiagCursor
        property real originalWidth:0
        property real initialX:0
        onPressed:mouse=>{originalWidth=root.width;initialX=mapToItem(root.parent,mouse.x,mouse.y).x}
        onPositionChanged:mouse=>{if(pressed){const current=mapToItem(root.parent,mouse.x,mouse.y).x;root.width=Math.max(240*VLCStyle.scale,Math.min(640*VLCStyle.scale,originalWidth+current-initialX));root.height=root.width*9/16+root.footerHeight}}
    }
    property int textStyle: Text.Outline

    // `hideControls` being `false` does not necessarily mean that the buttons are shown:
    property bool hideControls: MainCtx.usingTouch // By default controls are hidden with touch

    Accessible.role: Accessible.Graphic
    Accessible.focusable: false
    Accessible.name: qsTr("video content")

    Drag.active: dragHandler.active

    Drag.onActiveChanged: {
        root.anchors.left = undefined
        root.anchors.right = undefined
        root.anchors.top = undefined
        root.anchors.bottom = undefined
        root.anchors.verticalCenter = undefined
        root.anchors.horizontalCenter = undefined
    }

    DoubleClickIgnoringItem {
        anchors.fill: parent
        anchors.bottomMargin: root.footerHeight

        TapHandler {
            gesturePolicy: TapHandler.WithinBounds

            onDoubleTapped: (eventPoint, button) => {
                // Docs are lying, there is no `deviceType`: QTBUG-135104
                if (eventPoint.device.type !== PointerDevice.TouchScreen)
                    MainCtx.playerView = true
            }

            onTapped: (eventPoint, button) => {
                // Docs are lying, there is no `deviceType`: QTBUG-135104
                if (eventPoint.device.type === PointerDevice.TouchScreen) {
                    // Breaking the initial binding is intentional here:
                    root.hideControls = !root.hideControls
                } else {
                    MainPlaylistController.togglePlayPause()
                }
            }
        }

        DragHandler {
            id: dragHandler

            target: root

            cursorShape: Qt.DragMoveCursor

            dragThreshold: 0

            grabPermissions: PointerHandler.CanTakeOverFromAnything

            xAxis.minimum: root.dragXMin
            xAxis.maximum: root.dragXMax
            yAxis.minimum: root.dragYMin
            yAxis.maximum: root.dragYMax
        }

        HoverHandler {
            id: hoverHandler

            grabPermissions: PointerHandler.CanTakeOverFromAnything
            cursorShape: Qt.ArrowCursor

            Component.onCompleted: {
                if (typeof blocking === 'boolean')
                    blocking = true // Qt 6.3 feature
            }
        }
    }

    background: Rectangle {
        color: root.colorContext.bg.secondary
        radius: 12 * VLCStyle.scale
        border.color: root.colorContext.border
        VideoSurface {
            id: videoSurface
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 8 * VLCStyle.scale }
            height: root.height - root.footerHeight - 8 * VLCStyle.scale
            videoSurfaceProvider: MainCtx.videoSurfaceProvider

        color: (hoverHandler.hovered ||
                playButton?.hovered ||
                closeButton?.hovered ||
                fullscreenButton?.hovered) ? "#10000000" : "transparent"

        // It is acceptable for effects to exceed the boundaries of the
        // parent. They are an exception to the "hypothetical clip test".
            Widgets.DefaultShadow { }
        }
    }

    contentItem: Item {
      Item {
        anchors.fill: parent
        anchors.bottomMargin: root.footerHeight
        visible: !root.hideControls &&
                 (hoverHandler.hovered ||
                 playButton.hovered ||
                 closeButton.hovered ||
                 fullscreenButton.hovered)

        // Raise the content item so that the handlers of the control do
        // not handle events that are to be handled by the handlers/items
        // of the content item. Raising the content item should be fine
        // because content item is supposed to be the foreground item.
        z: 1

        Widgets.IconButton {
            id: playButton

            anchors.centerIn: parent

            font.pixelSize: VLCStyle.icon_large

            description: qsTr("play/pause")

            AccessibleCompat.id: "pipPlayButton"

            text: (Player.playingState !== Player.PLAYING_STATE_PAUSED
                   && Player.playingState !== Player.PLAYING_STATE_STOPPED)
                  ? VLCIcons.pause_filled
                  : VLCIcons.play_filled

            textStyle: root.textStyle

            onClicked: MainPlaylistController.togglePlayPause()
        }

        Widgets.IconButton {
            id: closeButton

            anchors {
                top: parent.top
                topMargin: VLCStyle.margin_small
                right: parent.right
                rightMargin: VLCStyle.margin_small
            }

            font.pixelSize: VLCStyle.icon_PIP
            description: qsTr("close video")

            AccessibleCompat.id: "pipCloseButton"

            text: VLCIcons.close

            textStyle: root.textStyle

            onClicked: MainPlaylistController.stop()
        }

        Widgets.IconButton {
            id: fullscreenButton

            anchors {
                top: parent.top
                topMargin: VLCStyle.margin_small
                left: parent.left
                leftMargin: VLCStyle.margin_small
            }

            font.pixelSize: VLCStyle.icon_PIP

            description: qsTr("maximize player")

            AccessibleCompat.id: "pipFullscreenButton"

            text: VLCIcons.fullscreen

            textStyle: root.textStyle

            onClicked: MainCtx.playerView = true
        }
      }
      ColumnLayout {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 * VLCStyle.scale }
        height: root.footerHeight - 20 * VLCStyle.scale
        spacing: 6 * VLCStyle.scale
        SliderBar {
            objectName: "redesignMiniSeek"
            Layout.fillWidth: true
            Layout.preferredHeight: 8 * VLCStyle.scale
            barHeight: 6 * VLCStyle.scale
            enabled: Player.seekable
        }
        Text {
            objectName: "redesignMiniTitle"
            Layout.fillWidth: true
            text: String(root.location.file || Player.name || Player.title || "")
            textFormat: Text.PlainText
            color: root.colorContext.fg.secondary
            font.family: "Inter"; font.pixelSize: 14 * VLCStyle.scale
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8 * VLCStyle.scale
            Widgets.IconToolButton {
                objectName: "redesignMiniPlay"
                text: Player.playingState === Player.PLAYING_STATE_PAUSED ? VLCIcons.play_filled : VLCIcons.pause_filled
                description: qsTr("Play or pause")
                onClicked: MainPlaylistController.togglePlayPause()
            }
            Text {
                Layout.fillWidth: true
                text: String(root.location.folder || Player.artist || "")
                textFormat: Text.PlainText
                color: root.colorContext.fg.secondary
                font.family: "Inter"; font.pixelSize: 12 * VLCStyle.scale
                elide: Text.ElideRight
            }
            Widgets.IconToolButton {
                text: VLCIcons.playlist; description: qsTr("Play queue")
                onClicked: MainCtx.playqueuePanel.visible = !MainCtx.playqueuePanel.visible
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: Player.time.formatHMS(0); color: root.colorContext.fg.secondary; font.pixelSize: 12 * VLCStyle.scale }
            Item { Layout.fillWidth: true }
            Text { text: Player.length.formatHMS(0); color: root.colorContext.fg.secondary; font.pixelSize: 12 * VLCStyle.scale }
        }
      }
    }
}
