// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import VLC.MainInterface
import VLC.Dialogs
import VLC.Playlist
import VLC.MediaLibrary
import VLC.Style

C.Popup {
    id:root;objectName:"redesignMediaDialog"
    property int mode:0 // files, network, disc, capture, convert, stream out
    property var files:[]
    property string error:""
    readonly property real unit:VLCStyle.scale
    readonly property ColorContext theme:ColorContext {colorSet:ColorContext.Window;palette:VLCStyle.palette}
    // same tokens as the other redesign panels (style/redesign_widgets.hpp)
    readonly property bool dark:theme.palette?.isDark ?? true
    readonly property color paper:dark?"#0e0e0e":"#f8f8f8"
    readonly property color panel:dark?"#191919":"#eaeaea"
    readonly property color field:dark?"#090909":"#ffffff"
    readonly property color ink:dark?"#8f8f8f":"#333333"
    readonly property color title:dark?"#c6c6c6":"#242424"
    readonly property color edge:dark?"#373737":"#cccccc"
    readonly property color muted:dark?"#6b6b6b":"#777777"
    readonly property color hover:dark?"#222222":"#dddddd"
    readonly property var titles:[qsTr("Open files"),qsTr("Open network stream"),qsTr("Open disc"),qsTr("Open capture device"),qsTr("Convert files"),qsTr("Stream to another device")]
    width:Math.min(800*unit,parent.width-24*unit);height:Math.min(580*unit,parent.height-24*unit)
    x:(parent.width-width)/2;y:(parent.height-height)/2;modal:true;focus:true;padding:24*unit
    palette.window:paper
    palette.windowText:ink
    palette.base:field
    palette.alternateBase:panel
    palette.button:panel
    palette.buttonText:ink
    palette.text:ink
    palette.placeholderText:muted
    palette.highlight:theme.accent
    palette.highlightedText:VLCStyle.onAccent(theme.accent)
    palette.light:edge
    palette.mid:edge
    palette.dark:edge
    closePolicy:C.Popup.CloseOnEscape
    background:Rectangle {color:root.paper;radius:12*root.unit;border.color:root.edge}

    component DialogButton: C.Button {
        id:control
        property bool primary:false
        implicitHeight:36*root.unit
        leftPadding:16*root.unit;rightPadding:16*root.unit
        font.pixelSize:14*root.unit
        contentItem:Text {text:control.text;font:control.font;color:control.primary?VLCStyle.onAccent(root.theme.accent):root.ink;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
        background:Rectangle {
            radius:6*root.unit
            color:control.primary?(control.down?Qt.darker(root.theme.accent,1.15):control.hovered?Qt.lighter(root.theme.accent,1.08):root.theme.accent)
                                 :(control.down||control.hovered?root.hover:root.panel)
            border.color:control.visualFocus?(control.primary?root.title:root.theme.accent):"transparent"
            border.width:2
        }
    }
    component DialogField: C.TextField {
        id:control
        implicitHeight:36*root.unit
        selectByMouse:true
        color:root.title;placeholderTextColor:root.muted
        selectionColor:root.theme.accent;selectedTextColor:VLCStyle.onAccent(root.theme.accent)
        background:Rectangle {color:root.field;radius:6*root.unit;border.width:2;border.color:control.activeFocus?root.theme.accent:(root.dark?"#191919":"#eaeaea")}
    }

    MLPlaylistListModel {id:playlists;ml:MediaLib;playlistType:MLPlaylistListModel.PLAYLIST_TYPE_ALL}
    function openMode(kind){mode=kind;files=[];error="";urls.text="";playlistName.text="";open()}
    function items(){return mode===1?urls.text.split(/\r?\n/).map(s=>s.trim()).filter(Boolean):files}
    function submit(){
        let items=root.items()
        if(mode===2){if(!discPath.text.trim()){error=qsTr("Choose the disc drive or folder.");return}items=[(discType.currentIndex===0?"dvdnav://":discType.currentIndex===1?"bluray://":"cdda://")+discPath.text.trim().replace(/\\/g,"/")]}
        if(mode===3){const values={};if(videoDevice.text.trim())values["dshow-vdev"]=videoDevice.text.trim();if(audioDevice.text.trim())values["dshow-adev"]=audioDevice.text.trim();if(!MainCtx.applyRedesignLiveOptions(values)){error=qsTr("The capture device could not be selected.");return}items=[captureType.currentIndex===0?"dshow://":"screen://"]}
        if(items.length===0){error=qsTr("Add a file or stream URL first.");return}
        if(mode===1&&items.some(s=>!/^\w[\w+.-]*:/.test(s))){error=qsTr("Enter a complete URL on each line.");return}
        if(mode>=4){DialogsProvider.redesignOutput(items,mode===5);close();return}
        if(playlistName.text.trim())playlists.create(playlistName.text.trim(),items)
        MainPlaylistController.append(items,!enqueue.checked)
        close()
    }
    contentItem:ColumnLayout {
        spacing:18*root.unit
        RowLayout {
            Layout.fillWidth:true
            Text {text:root.titles[root.mode];color:root.title;font.pixelSize:20*root.unit;Layout.fillWidth:true;elide:Text.ElideRight}
            C.Button {
                id:closeButton
                Accessible.name:qsTr("Close media dialog")
                implicitWidth:32*root.unit;implicitHeight:32*root.unit
                onClicked:root.close()
                contentItem:Text {text:VLCIcons.close;font.family:VLCIcons.fontFamily;font.pixelSize:24*root.unit;color:root.ink;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                background:Rectangle {radius:16*root.unit;color:closeButton.hovered?root.hover:"transparent";border.color:closeButton.visualFocus?root.theme.accent:"transparent"}
            }
        }
        C.ScrollView {
            Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
            ColumnLayout {
                width:parent.width;spacing:16*root.unit
                Text {visible:root.mode===1;text:qsTr("Network URLs");color:root.title;font.pixelSize:16*root.unit}
                Text {visible:root.mode===1;text:qsTr("Enter one link per line. VLC also opens stream playlists.");color:root.muted;Layout.fillWidth:true;wrapMode:Text.WordWrap}
                C.TextArea {id:urls;objectName:"redesignNetworkUrls";visible:root.mode===1;Layout.fillWidth:true;Layout.preferredHeight:170*root.unit;placeholderText:"https://example.com/stream.m3u8";placeholderTextColor:root.muted;selectByMouse:true;wrapMode:TextEdit.Wrap;color:root.title;selectionColor:root.theme.accent;selectedTextColor:VLCStyle.onAccent(root.theme.accent);background:Rectangle {color:root.field;radius:6*root.unit;border.width:2;border.color:urls.activeFocus?root.theme.accent:(root.dark?"#191919":"#eaeaea")}}
                DialogButton {visible:root.mode===0||root.mode>=4;text:qsTr("Select files…");onClicked:root.files=DialogsProvider.pickRedesignMediaFiles()}
                Repeater {model:root.mode===0||root.mode>=4?root.files:[];Text {required property var modelData;Layout.fillWidth:true;text:modelData;textFormat:Text.PlainText;elide:Text.ElideMiddle;color:root.ink}}
                C.ComboBox {id:discType;visible:root.mode===2;Layout.fillWidth:true;model:[qsTr("DVD"),qsTr("Blu-ray"),qsTr("Audio CD")]}
                RowLayout {visible:root.mode===2;Layout.fillWidth:true;DialogField {id:discPath;Layout.fillWidth:true;placeholderText:qsTr("Drive or disc folder (for example D:/)")} DialogButton {text:qsTr("Browse");onClicked:{const path=MainCtx.pickRedesignPath("redesign-media-folder",discPath.text);if(path)discPath.text=path}}}
                C.ComboBox {id:captureType;visible:root.mode===3;Layout.fillWidth:true;model:[qsTr("Camera or audio device"),qsTr("Desktop")];onActivated:root.error=""}
                DialogField {id:videoDevice;visible:root.mode===3&&captureType.currentIndex===0;Layout.fillWidth:true;placeholderText:qsTr("Video device name (blank for default)")}
                DialogField {id:audioDevice;visible:root.mode===3&&captureType.currentIndex===0;Layout.fillWidth:true;placeholderText:qsTr("Audio device name (blank for default)")}
                Text {visible:root.mode===0||root.mode===1;text:qsTr("Playlist name (optional)");color:root.title;font.pixelSize:16*root.unit}
                DialogField {id:playlistName;objectName:"redesignNetworkPlaylistName";visible:root.mode===0||root.mode===1;Layout.fillWidth:true;placeholderText:qsTr("Save these items as a playlist")}
                C.CheckBox {
                    id:enqueue;visible:root.mode<4;text:qsTr("Add to queue without playing")
                    indicator:Rectangle {
                        x:enqueue.leftPadding;y:(enqueue.height-height)/2
                        implicitWidth:18*root.unit;implicitHeight:18*root.unit;radius:4*root.unit
                        color:enqueue.checked?root.theme.accent:root.field
                        border.color:enqueue.visualFocus||enqueue.checked?root.theme.accent:root.edge
                        Text {anchors.centerIn:parent;visible:enqueue.checked;text:VLCIcons.check;font.family:VLCIcons.fontFamily;color:VLCStyle.onAccent(root.theme.accent);font.pixelSize:14*root.unit}
                    }
                    contentItem:Text {text:enqueue.text;color:root.ink;leftPadding:enqueue.indicator.width+8*root.unit;verticalAlignment:Text.AlignVCenter}
                }
                Text {text:root.error;color:"#e66161";visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.WordWrap}
            }
        }
        RowLayout {
            Layout.fillWidth:true;spacing:8*root.unit
            DialogButton {text:qsTr("Advanced options…");onClicked:{const kind=root.mode;root.close();DialogsProvider.classicMediaDialog(kind)}}
            Item {Layout.fillWidth:true}
            DialogButton {text:qsTr("Cancel");onClicked:root.close()}
            DialogButton {objectName:"redesignMediaSubmit";primary:true;text:root.mode>=4?qsTr("Next"):enqueue.checked?qsTr("Add"):qsTr("Play");onClicked:root.submit()}
        }
    }
}
