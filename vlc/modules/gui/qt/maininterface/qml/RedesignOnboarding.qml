// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Style

C.Popup {
    id: root
    objectName: "redesignOnboarding"
    width: Math.min(720 * VLCStyle.scale, parent.width * .82)
    height: Math.min(500 * VLCStyle.scale, parent.height - 80 * VLCStyle.scale)
    x: (parent.width-width)/2; y: (parent.height-height)/2
    readonly property bool compact: height < 430 * VLCStyle.scale || width < 560 * VLCStyle.scale
    readonly property real bodySize: 14 * VLCStyle.scale
    readonly property real titleSize: (compact ? 22 : 26) * VLCStyle.scale
    modal: true; focus: true; padding: (compact ? 16 : 28) * VLCStyle.scale
    z: 2 // stay above non-modal panels opened after the welcome
    closePolicy: C.Popup.CloseOnEscape
    C.Overlay.modal: Rectangle { color: "#30000000" }
    property int step: 0
    property int scheme: 2
    property string folder: ""
    property string error: ""
    readonly property ColorContext colorContext: ColorContext { palette: VLCStyle.palette; colorSet: ColorContext.Window }
    readonly property color ink: colorContext.palette.isDark ? "#969696" : "#333333"
    readonly property color paper: colorContext.palette.isDark ? "#0e0e0e" : "#f8f8f8"
    readonly property color field: colorContext.palette.isDark ? "#090909" : "#ffffff"
    readonly property color panel: colorContext.palette.isDark ? "#191919" : "#eaeaea"
    readonly property color hover: colorContext.palette.isDark ? "#222222" : "#dddddd"
    readonly property color edge: colorContext.palette.isDark ? "#222222" : "#cccccc"
    readonly property color line: colorContext.palette.isDark ? "#373737" : "#cccccc"
    // Figma folder row: field #090909 with a 2px #191919 border, Browse #1b1b1b with #8f8f8f text
    readonly property color fieldEdge: colorContext.palette.isDark ? "#191919" : "#cccccc"
    readonly property color browse: colorContext.palette.isDark ? "#1b1b1b" : "#eaeaea"
    readonly property color browseText: colorContext.palette.isDark ? "#8f8f8f" : "#333333"
    palette.window: paper; palette.base: field; palette.button: panel; palette.buttonText: ink; palette.text: ink
    palette.highlight: colorContext.accent; palette.highlightedText: VLCStyle.onAccent(colorContext.accent); palette.mid: line; palette.dark: line
    background: Rectangle { color: root.paper; radius: 14 * VLCStyle.scale; border.color: root.edge }
    component SecondaryButton: C.Button {
        implicitHeight: 32 * VLCStyle.scale
        leftPadding: 14 * VLCStyle.scale; rightPadding: 14 * VLCStyle.scale
        font.pixelSize: root.bodySize
        contentItem: Text { text: parent.text; font: parent.font; color: root.colorContext.fg.primary; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: parent.hovered ? root.hover : root.panel; radius: 6 * VLCStyle.scale; border.color: parent.visualFocus ? root.colorContext.accent : "transparent" }
    }
    C.ToolButton {
        objectName: "redesignOnboardingClose"
        parent: root.background
        x: parent.width - width - 8 * VLCStyle.scale; y: 8 * VLCStyle.scale
        width: 28 * VLCStyle.scale; height: width
        Accessible.name: qsTr("Close welcome")
        contentItem: Text { text: "×"; color: root.ink; font.pixelSize: 22 * VLCStyle.scale; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: parent.hovered ? root.hover : "transparent"; radius: 6 * VLCStyle.scale; border.color: parent.visualFocus ? root.colorContext.accent : "transparent" }
        onClicked: root.close()
    }
    function finish() {
        if (folder && (!MainCtx.mediaLibraryAvailable || !MainCtx.redesignFolderTree(folder))) { error = qsTr("Choose an available folder, or clear the folder to continue."); return }
        let values = {"qt-privacy-ask":false, "metadata-network-access":metadata.checked}
        if (MainCtx.redesignConfigValue("qt-updates-notif") !== undefined) values["qt-updates-notif"] = updates.checked
        if (!MainCtx.saveRedesignConfig(values)) { error = qsTr("VLC could not save these settings."); return }
        MainCtx.colorScheme.selectedIndex = scheme
        if (folder && !MainCtx.addRedesignMediaFolder(folder)) { error = qsTr("Choose an available folder, or clear the folder to continue."); return }
        if (MainCtx.mediaLibrary) MainCtx.mediaLibrary.reload()
        close()
    }
    contentItem: ColumnLayout {
        spacing: 12 * VLCStyle.scale
        C.ScrollView {
            id: bodyView
            objectName: "redesignOnboardingBody"
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth
            contentHeight: bodyColumn.implicitHeight
            clip: true
            ColumnLayout {
                id: bodyColumn
                width: bodyView.availableWidth
                y: Math.max(0, (bodyView.availableHeight - height) / 2)
                spacing: 12 * VLCStyle.scale
                Image { objectName: "redesignOnboardingLogo"; visible: root.step === 0; source: "qrc:/redesign/onboarding-imgFrame4.svg"; Layout.preferredHeight: (root.compact ? 56 : 80) * VLCStyle.scale; Layout.preferredWidth: Layout.preferredHeight; Layout.alignment: Qt.AlignHCenter; fillMode: Image.PreserveAspectFit }
                Text { objectName: "redesignOnboardingTitle"; text: [qsTr("Welcome to VLC!"),qsTr("Make VLC yours"),qsTr("Your privacy"),qsTr("Choose a Media Folder")][root.step]; color: root.colorContext.fg.primary; font.pixelSize: root.titleSize; font.weight: Font.Light; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text {
                    visible: root.step === 0
                    objectName: "redesignOnboardingDescription"
                    text: qsTr("Play videos, music and streams. Choose your look and add a media folder, or close this welcome to start using VLC.")
                    color: root.ink; font.pixelSize: root.bodySize; lineHeight: 1.3; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true; Layout.alignment: Qt.AlignHCenter
                }
                RowLayout {
                    visible: root.step === 1; Layout.fillWidth: true; spacing: 12 * VLCStyle.scale
                    Repeater {
                        model: [{name:qsTr("Dark"),value:2,bg:"#0e0e0e",fg:"#969696"},{name:qsTr("Light"),value:1,bg:"#f8f8f8",fg:"#333333"},{name:qsTr("System"),value:0,bg:root.paper,fg:root.ink}]
                        C.Button {
                            required property var modelData
                            Layout.fillWidth: true; Layout.preferredHeight: (root.compact ? 84 : 120) * VLCStyle.scale
                            background: Rectangle { color: parent.modelData.bg; radius: 12 * VLCStyle.scale; border.color: root.scheme === parent.modelData.value || parent.visualFocus ? root.colorContext.accent : root.line; border.width: 2 }
                            contentItem: ColumnLayout { Rectangle { color: root.colorContext.accent; Layout.preferredWidth: 54 * VLCStyle.scale; Layout.preferredHeight: 28 * VLCStyle.scale; Layout.alignment: Qt.AlignHCenter; radius: 6 * VLCStyle.scale } Text { text: parent.parent.modelData.name; color: parent.parent.modelData.fg; font.pixelSize: 16 * VLCStyle.scale; Layout.alignment: Qt.AlignHCenter } }
                            onClicked: root.scheme = modelData.value
                        }
                    }
                }
                ColumnLayout {
                    visible: root.step === 2; Layout.fillWidth: true; spacing: 8 * VLCStyle.scale
                    Text { text: qsTr("Optional online services can fetch cover art and track details. Requests may identify your media to those services."); color: root.ink; font.pixelSize: root.bodySize; wrapMode: Text.WordWrap; Layout.fillWidth: true; lineHeight: 1.3 }
                    C.CheckBox { id: metadata; objectName: "redesignOnboardingMetadata"; text: qsTr("Allow automatic metadata retrieval"); checked: false; Layout.fillWidth: true
                        contentItem: Text { text: metadata.text; leftPadding: metadata.indicator.width + metadata.spacing; color: root.ink; font.pixelSize: root.bodySize; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter }
                    }
                    C.CheckBox { id: updates; objectName: "redesignOnboardingUpdates"; text: qsTr("Notify me when VLC updates are available"); checked: false; visible: MainCtx.redesignConfigValue("qt-updates-notif") !== undefined; Layout.fillWidth: true
                        contentItem: Text { text: updates.text; leftPadding: updates.indicator.width + updates.spacing; color: root.ink; font.pixelSize: root.bodySize; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter }
                    }
                }
                ColumnLayout {
                    visible: root.step === 3; Layout.fillWidth: true
                    Text { text: qsTr("Add a folder now, or add folders later in Preferences."); color: root.ink; font.pixelSize: root.bodySize; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        spacing: 8 * VLCStyle.scale
                        C.TextField {
                            text: root.folder; placeholderText: qsTr("Choose a folder (optional)"); Layout.fillWidth: true; onEditingFinished: root.folder = text
                            implicitHeight: 36 * VLCStyle.scale; leftPadding: 12 * VLCStyle.scale; font.pixelSize: root.bodySize
                            color: root.colorContext.fg.primary; placeholderTextColor: root.ink
                            selectionColor: root.colorContext.accent; selectedTextColor: VLCStyle.onAccent(root.colorContext.accent)
                            background: Rectangle { color: root.field; radius: 6 * VLCStyle.scale; border.width: 2; border.color: parent.activeFocus ? root.colorContext.accent : root.fieldEdge }
                        }
                        C.Button {
                            text: qsTr("Browse")
                            Layout.preferredWidth: Math.max(114 * VLCStyle.scale, implicitWidth); Layout.preferredHeight: 36 * VLCStyle.scale
                            font.pixelSize: root.bodySize
                            contentItem: Text { text: parent.text; font: parent.font; color: parent.hovered ? root.colorContext.fg.primary : root.browseText; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: parent.hovered ? root.hover : root.browse; radius: 6 * VLCStyle.scale; border.color: parent.visualFocus ? root.colorContext.accent : "transparent" }
                            onClicked: { const path = MainCtx.pickRedesignPath("redesign-media-folder",root.folder); if (path) root.folder = path }
                        }
                    }
                }
                Text { visible: root.error.length > 0; text: root.error; color: root.colorContext.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        RowLayout {
            objectName: "redesignOnboardingFooter"
            Layout.fillWidth: true
            SecondaryButton { text: qsTr("Back"); visible: root.step > 0; Layout.preferredWidth: Math.max(72 * VLCStyle.scale, implicitWidth); Layout.preferredHeight: 32 * VLCStyle.scale; onClicked: root.step-- }
            // keep the step dots centred whether or not Back is shown
            Item { visible: root.step === 0; Layout.preferredWidth: 72 * VLCStyle.scale }
            Item { Layout.fillWidth: true }
            Row { spacing: 6 * VLCStyle.scale; Repeater { model: 4; Rectangle { required property int index; width: 6 * VLCStyle.scale; height: width; radius: width/2; color: root.step === index ? root.colorContext.accent : root.line } } }
            Item { Layout.fillWidth: true }
            C.Button {
                objectName: "redesignOnboardingNext"
                text: root.step === 3 ? qsTr("Finish") : qsTr("Next")
                Layout.preferredWidth: 100 * VLCStyle.scale; Layout.preferredHeight: 32 * VLCStyle.scale
                contentItem: Text { text: parent.text; color: VLCStyle.onAccent(root.colorContext.accent); font.pixelSize: 16 * VLCStyle.scale; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? Qt.darker(root.colorContext.accent, 1.15) : parent.hovered ? Qt.lighter(root.colorContext.accent, 1.08) : root.colorContext.accent; radius: 6 * VLCStyle.scale; border.color: parent.visualFocus ? root.colorContext.fg.primary : "transparent" }
                onClicked: { if (root.step < 3) root.step++; else root.finish() }
            }
        }
    }
}
