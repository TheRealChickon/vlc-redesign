// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as C
import VLC.MainInterface
import VLC.Player
import VLC.Playlist
import VLC.Style

C.Popup {
    id:root
    objectName:"redesignFolderPanel"
    property string folder:""
    property var selected:[]
    property var history:[]
    property var entries:[]
    readonly property real unit:VLCStyle.scale
    readonly property ColorContext theme:ColorContext {colorSet:ColorContext.View;palette:VLCStyle.palette}
    width:Math.min(368*unit,parent.width-24*unit)
    height:Math.max(180*unit,parent.height-130*unit)
    padding:12*unit;focus:true;modal:false
    palette.window:theme.bg.primary
    palette.base:theme.bg.secondary
    palette.button:theme.bg.secondary
    palette.buttonText:theme.fg.primary
    palette.text:theme.fg.primary
    palette.placeholderText:theme.fg.secondary
    palette.highlight:theme.accent
    palette.highlightedText:theme.fg.primary
    palette.light:theme.border
    palette.mid:theme.border
    closePolicy:C.Popup.CloseOnEscape
    background:Rectangle {color:root.theme.bg.primary;radius:8*root.unit;border.color:root.theme.border}
    function navigate(path,remember=true){if(remember&&folder&&folder!==path)history=history.concat([folder]);folder=path;selected=[];entries=MainCtx.redesignBrowseFolder(path)}
    function toggle(url){selected=selected.includes(url)?selected.filter(s=>s!==url):selected.concat([url])}
    function breadcrumbs(){const path=folder.replace(/\\/g,"/");let parts=path.split("/").filter(Boolean);return parts.map((name,index)=>({name:name,path:parts.slice(0,index+1).join("/")+(index===0?"/":"")}))}
    onOpened:{if(!folder){const current=MainCtx.redesignCurrentFolder();if(current)navigate(current,false)}else entries=MainCtx.redesignBrowseFolder(folder)}
    contentItem:ColumnLayout {
        spacing:10*root.unit
        RowLayout {
            Layout.fillWidth:true
            C.Button {text:"‹";enabled:root.history.length>0;Accessible.name:qsTr("Previous folder");onClicked:{const previous=root.history[root.history.length-1];root.history=root.history.slice(0,-1);root.navigate(previous,false)}}
            Text {text:qsTr("Videos & folders");color:root.theme.fg.primary;Layout.fillWidth:true;font.pixelSize:15*root.unit}
            C.Button {text:"×";Accessible.name:qsTr("Close folder library");onClicked:root.close()}
        }
        ListView {
            Layout.fillWidth:true;Layout.preferredHeight:30*root.unit;orientation:ListView.Horizontal;clip:true
            model:root.breadcrumbs()
            onCountChanged:positionViewAtEnd()
            delegate:C.Button {required property var modelData;text:modelData.name+" ›";flat:true;width:Math.min(140*root.unit,implicitWidth);height:30*root.unit
                contentItem:Text {text:parent.text;color:root.theme.fg.primary;font.pixelSize:12*root.unit;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
                onClicked:root.navigate(modelData.path)}
        }
        C.TextField {id:search;Layout.fillWidth:true;placeholderText:qsTr("Search this folder");selectByMouse:true}
        ListView {
            id:list;objectName:"redesignFolderEntries"
            Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            model:root.entries.filter(e=>e.name.toLowerCase().includes(search.text.toLowerCase()))
            C.ScrollBar.vertical:C.ScrollBar {}
            delegate:C.ItemDelegate {
                id:entry;required property var modelData
                width:list.width;height:52*root.unit
                contentItem:RowLayout {
                    C.CheckBox {visible:!entry.modelData.folder;checked:root.selected.includes(entry.modelData.url);onClicked:root.toggle(entry.modelData.url);Accessible.name:qsTr("Select %1").arg(entry.modelData.name)}
                    Text {text:entry.modelData.folder?"▱":"▷";color:root.theme.accent;font.pixelSize:20*root.unit}
                    Text {text:entry.modelData.name;textFormat:Text.PlainText;color:root.theme.fg.primary;elide:Text.ElideRight;Layout.fillWidth:true;font.pixelSize:13*root.unit}
                    C.Button {text:"+";visible:!entry.modelData.folder;Accessible.name:qsTr("Add %1 to queue").arg(entry.modelData.name);onClicked:MainPlaylistController.append([entry.modelData.url],false)}
                }
                onClicked:{if(modelData.folder)root.navigate(modelData.path);else MainPlaylistController.append([modelData.url],true)}
                Drag.active:drag.active
                Drag.dragType:Drag.Automatic;Drag.supportedActions:Qt.CopyAction
                Drag.mimeData:({"text/uri-list":(root.selected.includes(modelData.url)?root.selected:[modelData.url]).join("\r\n")})
                DragHandler {id:drag;target:null;enabled:!entry.modelData.folder}
            }
            footer:Text {visible:list.count===0;width:list.width;text:root.folder?qsTr("No matching media in this folder."):qsTr("Choose a folder to browse.");wrapMode:Text.WordWrap;color:root.theme.fg.secondary}
        }
        RowLayout {
            Layout.fillWidth:true
            C.Button {text:qsTr("Choose folder");Layout.fillWidth:true;onClicked:{const path=MainCtx.pickRedesignPath("redesign-media-folder",root.folder);if(path)root.navigate(path)}}
            C.Button {text:qsTr("Queue");onClicked:MainCtx.askShowRedesignQueue(1)}
        }
        C.Button {text:qsTr("Add selected to queue (%1)").arg(root.selected.length);enabled:root.selected.length>0;Layout.fillWidth:true;onClicked:MainPlaylistController.append(root.selected,false)}
        C.Button {text:qsTr("Full library");Layout.fillWidth:true;onClicked:{root.close();MainCtx.playerView=false;History.push(["video"])}}
    }
}
