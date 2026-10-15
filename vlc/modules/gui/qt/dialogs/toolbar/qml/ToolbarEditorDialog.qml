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
import QtQuick.Templates as T
import QtQuick.Controls.Basic as C
import QtQuick.Layouts

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Dialogs


WindowDialog {
    id: root
    objectName: "redesignToolbarEditor"

    width: Math.min(1048 * VLCStyle.scale, Screen.desktopAvailableWidth - 32,
                    transientParent ? transientParent.width - 32 : 1048 * VLCStyle.scale)
    height: Math.min(865 * VLCStyle.scale, Screen.desktopAvailableHeight - 32,
                     transientParent ? transientParent.height - 32 : 865 * VLCStyle.scale)

    minimumWidth: Math.min(640 * VLCStyle.scale, width)
    minimumHeight: 400

    modal: true
    title: qsTr("Customize Player")
    redesignFrame: true
    standardButtons: C.DialogButtonBox.Save | C.DialogButtonBox.Cancel
    description: qsTr("Drag controls into the player to add them. Drag them back into the box below to remove them, or drag them along the player to change their order.")

    footerComponent: RowLayout {
        implicitHeight: 32 * VLCStyle.scale
        Widgets.ButtonExt {
            id: cancelButton
            text: qsTr("Cancel")
            implicitWidth: 100 * VLCStyle.scale
            implicitHeight: 32 * VLCStyle.scale
            font.pixelSize: 14 * VLCStyle.scale
            font.weight: Font.DemiBold
            color: root.colorContext.fg.secondary
            onClicked: root.rejected(true)
            background: Rectangle {
                radius: 6 * VLCStyle.scale
                color: root.colorContext.palette.isDark ? "#1b1b1b" : "#eaeaea"
                border.color: cancelButton.visualFocus ? root.colorContext.accent : "transparent"
            }
        }
        Item { Layout.fillWidth: true }
        Widgets.ButtonExt {
            id: saveButton
            text: qsTr("Save")
            implicitWidth: 100 * VLCStyle.scale
            implicitHeight: 32 * VLCStyle.scale
            font.pixelSize: 14 * VLCStyle.scale
            font.weight: Font.DemiBold
            color: root.colorContext.accent
            onClicked: root.accepted()
            background: Rectangle {
                radius: 6 * VLCStyle.scale
                color: root.colorContext.palette.isDark ? Qt.darker(root.colorContext.accent, 4) : Qt.lighter(root.colorContext.accent, 1.9)
                border.color: saveButton.visualFocus ? root.colorContext.accent : "transparent"
            }
        }
    }

    signal unload()
    property var originalSettings: ({})

    Component.onCompleted: {
        // Save first, in case the dialog is rejected.
        MainCtx.controlbarProfileModel.save(false)
        originalSettings = MainCtx.redesignInterfaceState()
    }

    onAccepted: {
        MainCtx.controlbarProfileModel.save()
        const profile = MainCtx.controlbarProfileModel.currentModel
        if (profile) MainCtx.storeRedesignInterfaceProfile(profile.redesignId)
        unload()
    }

    onRejected: (byButton) => {
        // Load saved to discard the changes
        MainCtx.controlbarProfileModel.reload()
        MainCtx.applyRedesignInterfaceState(originalSettings, false)
        VLCStyle.palette.refreshAccent()
        VLCStyle.darkPalette.refreshAccent()
        unload()
    }

    function newProfile(profileCreatorFunction) {
        console.assert(typeof profileCreatorFunction === 'function')

        const count = MainCtx.controlbarProfileModel.rowCount()
        const npDialog = DialogsProvider.getTextDialog(null,
                                                       qsTr("Profile Name"),
                                                       qsTr("Please enter the new profile name:"),
                                                       qsTr("Profile %1").arg(count + 1))
        if (!npDialog.ok)
            return

        profileCreatorFunction(npDialog.text)
        MainCtx.controlbarProfileModel.selectedProfile = count
    }

    function _markDirty(text) {
        return (text += " *")
    }

    contentComponent: Item {
        MouseArea {
            anchors.fill: parent

            cursorShape: toolbarEditor.dragActive ? Qt.ForbiddenCursor : Qt.ArrowCursor
        }

        ColumnLayout {
            anchors.fill: parent

            spacing: VLCStyle.margin_small

            RowLayout {
                Layout.fillHeight: false

                spacing: VLCStyle.margin_xsmall

                Widgets.MenuLabel {
                    Layout.fillWidth: true
                    Layout.minimumWidth: implicitWidth

                    color: root.colorContext.fg.primary
                    text: qsTr("Select profile:")
                }

                Widgets.ComboBoxExt {
                    id: comboBox

                    Layout.minimumWidth: VLCStyle.combobox_width_large
                    Layout.preferredWidth: Math.min(Math.max(implicitWidth, Layout.minimumWidth), VLCStyle.dp(280, VLCStyle.scale))
                    Layout.minimumHeight: VLCStyle.combobox_height_normal

                    // this is not proper way to do it,
                    // but ComboBoxExt does not provide
                    // correct implicit width:
                    implicitWidth: implicitContentWidth + leftPadding + rightPadding + indicator.width + spacing
                    font.pixelSize: VLCStyle.fontSize_normal

                    delegate: T.ItemDelegate {
                        id: profileEntry
                        readonly property bool current: comboBox.currentIndex === index
                        width: comboBox.width - VLCStyle.dp(8, VLCStyle.scale)
                        leftPadding: VLCStyle.dp(10, VLCStyle.scale)
                        rightPadding: VLCStyle.dp(10, VLCStyle.scale)
                        topPadding: VLCStyle.margin_xsmall
                        bottomPadding: VLCStyle.margin_xsmall
                        background: Rectangle {
                            radius: VLCStyle.dp(4, VLCStyle.scale)
                            color: profileEntry.highlighted || profileEntry.current ? Qt.alpha(comboBox.color, 0.08) : "transparent"
                        }
                        contentItem: Row {
                            spacing: VLCStyle.margin_xsmall
                            Text {
                                width: profileEntry.availableWidth - profileCheck.width - parent.spacing
                                text: model.dirty ? _markDirty(model.name)
                                                  : model.name
                                color: comboBox.color
                                font: comboBox.font
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                id: profileCheck
                                text: "✓"
                                visible: profileEntry.current
                                color: comboBox.colorContext.accent
                                font.pixelSize: VLCStyle.dp(16, VLCStyle.scale)
                            }
                        }
                        highlighted: (comboBox.highlightedIndex === index)
                    }

                    displayText: {
                        let text

                        if (!!MainCtx.controlbarProfileModel.currentModel)
                            text = MainCtx.controlbarProfileModel.currentModel.name
                        else {
                            text = "N/A"
                            return text
                        }

                        if (MainCtx.controlbarProfileModel.currentModel.dirty)
                            return _markDirty(text)
                        else
                            return text
                    }

                    model: MainCtx.controlbarProfileModel

                    currentIndex: MainCtx.controlbarProfileModel.selectedProfile
                    onActivated: {
                        const profile = MainCtx.controlbarProfileModel.currentModel
                        if (profile) MainCtx.loadRedesignInterfaceProfile(profile.redesignId)
                        VLCStyle.palette.refreshAccent()
                        VLCStyle.darkPalette.refreshAccent()
                    }

                    onCurrentIndexChanged: {
                        MainCtx.controlbarProfileModel.selectedProfile = currentIndex
                    }

                    Accessible.name: qsTr("Profiles")
                }

                Widgets.ButtonExt {
                    text: qsTr("Rename")
                    implicitWidth: 76 * VLCStyle.scale
                    Accessible.name: qsTr("Rename the selected profile")
                    onClicked: {
                        const profile = MainCtx.controlbarProfileModel.currentModel
                        if (!profile) return
                        const rename = DialogsProvider.getTextDialog(null,qsTr("Profile Name"),qsTr("Enter the profile name:"),profile.name)
                        if (rename.ok && rename.text.trim()) profile.name = rename.text.trim()
                    }
                }
                Widgets.IconToolButton {
                    description: qsTr("Clone the selected profile")
                    text: VLCIcons.ic_fluent_document_copy_24_regular

                    onClicked: {
                        root.newProfile(MainCtx.controlbarProfileModel.cloneSelectedProfile)
                    }
                }

                Widgets.IconToolButton {
                    description: qsTr("New profile")
                    text: VLCIcons.ic_fluent_document_add_24_regular

                    onClicked: {
                        root.newProfile(MainCtx.controlbarProfileModel.newProfile)
                    }
                }

                Widgets.IconToolButton {
                    description: qsTr("Delete the current profile.\nDeleting all profiles brings back the default styles.")
                    text: VLCIcons.del

                    onClicked: {
                          MainCtx.controlbarProfileModel.deleteSelectedProfile()
                    }
                }
            }

            // The main context of the toolbareditor dialog:
            ToolbarEditor {
                id: toolbarEditor

                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
