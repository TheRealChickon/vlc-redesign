/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Controls.Basic
import QtQuick.Templates as T


import VLC.MainInterface
import VLC.Style
import VLC.Widgets as Widgets
import VLC.Util

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + spacing + implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: VLCStyle.margin_xxsmall
    font.pixelSize: VLCStyle.fontSize_large

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.ComboBox

        focused: control.activeFocus
        enabled: control.enabled
        hovered: control.hovered
        pressed: control.pressed
    }

    property color color: theme.fg.primary
    property color bgColor: theme.bg.primary
    property color borderColor: theme.border

    Keys.priority: Keys.AfterItem
    Keys.onPressed: (event) => Navigation.defaultKeyAction(event)

    // redesign: filled field with the Figma chevron and a rounded list marking the current row
    leftPadding: VLCStyle.dp(12, VLCStyle.scale)
    rightPadding: VLCStyle.dp(8, VLCStyle.scale)

    delegate: T.ItemDelegate {
        id: entry
        readonly property bool current: control.currentIndex === index
        width: control.width - VLCStyle.dp(8, VLCStyle.scale)
        height: implicitContentHeight + topPadding + bottomPadding
        padding: VLCStyle.margin_xsmall
        leftPadding: VLCStyle.dp(10, VLCStyle.scale)
        rightPadding: VLCStyle.dp(10, VLCStyle.scale)
        contentItem: Row {
            spacing: VLCStyle.margin_xsmall
            Widgets.ListLabel {
                width: entry.availableWidth - check.width - parent.spacing
                text: control.textRole ? (Helpers.isArray(control.model) ? modelData[control.textRole] : model[control.textRole]) : modelData
                color: control.color
                opacity: entry.current ? 1 : .8
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: check
                text: "✓"
                visible: entry.current
                color: theme.accent
                font.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        background: Rectangle {
            radius: VLCStyle.dp(4, VLCStyle.scale)
            color: entry.highlighted || entry.current ? Qt.alpha(control.color, 0.08) : "transparent"
        }
        highlighted: control.highlightedIndex === index
    }

    indicator: Image {
        x: control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: VLCStyle.dp(24, VLCStyle.scale)
        height: width
        sourceSize: Qt.size(width, height)
        source: VLCStyle.iconUrl("settings-imgFrame.svg", control.color)
        rotation: control.popup.visible ? 180 : 0
    }

    contentItem: Widgets.ListLabel {
        rightPadding: control.spacing + control.indicator.width

        text: control.displayText
        color: control.color
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Widgets.AnimatedBackground {
        enabled: theme.initialized
        color: Qt.alpha(control.color, control.hovered ? 0.1 : 0.06)
        border.color: control.visualFocus ? theme.visualFocus : "transparent"
        border.width: VLCStyle.dp(2, VLCStyle.scale)
        radius: VLCStyle.dp(6, VLCStyle.scale)
    }

    popup: Popup {
        y: control.height + VLCStyle.dp(4, VLCStyle.scale)

        // NOTE: This Popup should be on top of other Popup(s) most of the time.
        z: 100

        width: control.width
        padding: VLCStyle.dp(4, VLCStyle.scale)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: control.bgColor
            border.color: Qt.alpha(control.color, 0.12)
            border.width: VLCStyle.dp(1, VLCStyle.scale)
            radius: VLCStyle.dp(8, VLCStyle.scale)
        }
    }
}
