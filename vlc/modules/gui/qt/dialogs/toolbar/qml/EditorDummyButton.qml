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
import QtQuick.Templates as T


import VLC.Style
import VLC.MainInterface

T.Control {
    id: root
    property string text: ""
    property color color: "white"
    // redesign SVGs carry their own padding, so they need a larger box than font glyphs
    property real imageSize: 24 * VLCStyle.scale
    font.pixelSize: VLCStyle.icon_medium
    font.family: VLCIcons.fontFamily

    contentItem: Item {
        Image { anchors.centerIn: parent; width: Math.min(parent.width, root.imageSize); height: width; source: VLCStyle.iconUrl(VLCStyle.iconAsset(root.text), root.color); visible: VLCStyle.iconAsset(root.text).length > 0; fillMode: Image.PreserveAspectFit }
        Text { anchors.fill: parent; text: root.text; color: root.color; font: root.font; visible: VLCStyle.iconAsset(root.text).length === 0; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
}
