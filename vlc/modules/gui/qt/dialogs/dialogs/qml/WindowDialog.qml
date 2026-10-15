/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQuick.Window
import QtQuick.Controls.Basic
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style

Window {
    id: root

    transientParent: MainCtx.intfMainWindow

    property bool redesignFrame: false
    property string description: ""
    flags: Qt.Dialog | (redesignFrame ? Qt.FramelessWindowHint : 0)

    property bool modal: false
    modality: modal ? Qt.ApplicationModal : Qt.NonModal

    width: VLCStyle.appWidth * 0.75
    height: VLCStyle.appHeight * 0.85

    color: redesignFrame ? "transparent" : theme.bg.primary

    property alias contentComponent: loader.sourceComponent
    property Component footerComponent: null
    property alias standardButtons: buttonBox.standardButtons
    property alias colorContext: layout.colorContext

    signal accepted()
    signal rejected(bool byButton)
    signal applied()
    signal discarded()
    signal reset()

    onAccepted: hide()
    onRejected: (byButton) => {
        if (byButton)
            hide()
    }
    onApplied: hide()
    onDiscarded: hide()
    onReset: hide()

    onClosing: {
        rejected(false)
    }

    function open() {
        show()
    }

    Component.onCompleted: {
        if (MainCtx.createWindowWithoutRedirectionSurface) { // Win32
            const ret = MainCtx.createWindowWithoutRedirectionSurface(this)
            if (!ret)
                console.debug("MainCtx::createWindowWithoutRedirectionSurface(): returned false for window", this)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.bg.primary
        radius: root.redesignFrame ? 14 * VLCStyle.scale : 0
        border.color: root.redesignFrame ? theme.border : "transparent"
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: root.redesignFrame ? 0 : VLCStyle.margin_small


        readonly property ColorContext colorContext: ColorContext {
            palette: VLCStyle.palette
            colorSet: ColorContext.Window
        }

        ColumnLayout {
            visible: root.redesignFrame
            Layout.fillWidth: true
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 50 * VLCStyle.scale
                color: theme.palette.isDark ? "#090909" : "#eaeaea"
                radius: 14 * VLCStyle.scale
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 20 * VLCStyle.scale; anchors.rightMargin: 9 * VLCStyle.scale
                    Text { text: root.title; color: theme.fg.secondary; font.pixelSize: 14 * VLCStyle.scale; Layout.fillWidth: true }
                    Widgets.IconToolButton { text: VLCIcons.close; description: qsTr("Close"); onClicked: root.rejected(true) }
                }
                DragHandler { target: null; acceptedButtons: Qt.LeftButton; onActiveChanged: if (active) root.startSystemMove() }
            }
            Text {
                visible: root.description.length > 0
                text: root.description; color: theme.fg.secondary; wrapMode: Text.WordWrap
                font.pixelSize: 14 * VLCStyle.scale
                Layout.fillWidth: true; Layout.leftMargin: 20 * VLCStyle.scale; Layout.rightMargin: 20 * VLCStyle.scale
                Layout.topMargin: 12 * VLCStyle.scale; Layout.bottomMargin: 12 * VLCStyle.scale
            }
        }

        Loader {
            id: loader
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.leftMargin: root.redesignFrame ? 34 * VLCStyle.scale : 0
            Layout.rightMargin: root.redesignFrame ? 34 * VLCStyle.scale : 0

            clip: true
            sourceComponent: contentComponent
        }

        Loader {
            active: root.footerComponent !== null
            visible: active
            sourceComponent: root.footerComponent
            Layout.fillWidth: true
            Layout.leftMargin: 20 * VLCStyle.scale
            Layout.rightMargin: 20 * VLCStyle.scale
            Layout.bottomMargin: 20 * VLCStyle.scale
        }

        DialogButtonBox {
            id: buttonBox
            visible: !root.footerComponent

            padding: 0
            spacing: VLCStyle.margin_small

            Layout.fillWidth: true
            Layout.minimumHeight: VLCStyle.icon_normal

            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
            alignment: Qt.AlignRight
            Layout.leftMargin: root.redesignFrame ? 24 * VLCStyle.scale : 0
            Layout.rightMargin: root.redesignFrame ? 24 * VLCStyle.scale : 0
            Layout.bottomMargin: root.redesignFrame ? 24 * VLCStyle.scale : 0

            background: null

            onAccepted: root.accepted()
            onRejected: root.rejected(true)
            onApplied: root.applied()
            onDiscarded: root.discarded()
            onReset: root.reset()


            delegate: Widgets.ButtonExt {
                implicitWidth: 76 * VLCStyle.scale
                implicitHeight: 34 * VLCStyle.scale
            }
        }
    }
}
