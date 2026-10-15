// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import VLC.Player
import VLC.Style
import VLC.Util
LangButton {
    text: VLCIcons.effect_filter
    initialTab: 0
    description: qsTr("Playback settings")
    AccessibleCompat.id: "extendedSettingsButton"
}
