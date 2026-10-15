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


import VLC.Widgets as Widgets
import VLC.Style
import VLC.Player
import VLC.Util

Widgets.IconToolButton {
    id: root
    objectName: "redesignTracksButton"
    property int initialTab: 3
    readonly property var livePanel: menuLoader.item
    function showPanel(tab: int) { menuLoader.item.currentTab = tab; menuLoader.item.open() }
    function showSubtitleImport() { menuLoader.item.showSubtitleImport() }
    function closeSubtitleImport() { menuLoader.item.closeSubtitleImport() }
    function closePanel() { menuLoader.item.close() }

    // Proprerties

    readonly property var _parentItem: {
        if ((typeof rootPlayer !== 'undefined') && (rootPlayer !== null))
            return rootPlayer
        else
            return g_mainDisplay
    }

    // Signals

    signal requestLockUnlockAutoHide(bool lock)
    signal menuOpened(var menu)

    // Settings

    text: VLCIcons.audiosub

    enabled: menuLoader.status === Loader.Ready

    description: qsTr("Languages and tracks")

    AccessibleCompat.id: "langButton"

    // Events

    onClicked: { menuLoader.item.currentTab = initialTab; menuLoader.item.open() }

    // Children

    Loader {
        id: menuLoader

        sourceComponent: TracksMenu {
            id: menu

            parent: root._parentItem

            x: Math.max(0, parent.width - width - VLCStyle.margin_small)
            y: Math.max(VLCStyle.dp(40, VLCStyle.scale), parent.positionSliderY - height - VLCStyle.margin_small)
            z: 1

            focus: true

            colorContext.palette: root.colorContext.palette

            onOpened: {
                root.requestLockUnlockAutoHide(true)
                root.menuOpened(menu)
            }

            onClosed: {
                root.requestLockUnlockAutoHide(false)
                root.forceActiveFocus()
                root.menuOpened(null)
            }
        }
    }

    function forceUnlock() {
        if((menuLoader)&&(menuLoader.item))
            menuLoader.item.close()
    }
}
