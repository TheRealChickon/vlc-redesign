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
import QtQuick.Layouts
import QtQuick.Controls.Basic as C

import VLC.Player
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.MainInterface

T.ProgressBar {
    id: control

    property bool redesignBlock: false
    property var waveformPeaks: []
    readonly property real _hoveredScalingFactor: redesignBlock ? 1 : 1.8
    property int barHeight: VLCStyle.dp(5, VLCStyle.scale)
    readonly property real _scaledBarHeight: control.barHeight * _hoveredScalingFactor
    readonly property real _scaledY: (-control.barHeight / 2) * (control._hoveredScalingFactor - 1)

    property bool _isSeekPointsShown: true
    readonly property int _seekPointsDistance: VLCStyle.dp(2, VLCStyle.scale)
    readonly property int _seekPointsRadius: VLCStyle.dp(0.5, VLCStyle.scale)
    readonly property real _scaledSeekPointsRadius: _seekPointsRadius * _hoveredScalingFactor

    objectName: redesignBlock ? "redesignSeekBar" : "nativeSeekBar"
    readonly property real previewPosition: hoverHandler.hovered ? Helpers.clamp(hoverHandler.point.position.x / Math.max(1, width), 0, 1) : value
    onPreviewPositionChanged: if (redesignBlock && MainCtx.redesignPresentation.preview && hoverHandler.hovered && Player.hasVideoOutput) seekPreviewTimer.restart()
    Timer { id: seekPreviewTimer; interval: 220; onTriggered: MainCtx.requestRedesignSeekPreview(control.previewPosition) }
    Connections {
        target: hoverHandler
        function onHoveredChanged() {
            if (hoverHandler.hovered && control.redesignBlock && MainCtx.redesignPresentation.preview && Player.hasVideoOutput) seekPreviewTimer.restart()
            else { seekPreviewTimer.stop(); MainCtx.cancelRedesignSeekPreview() }
        }
    }
    C.Popup {
        id: seekPreview
        objectName: "redesignSeekPreview"
        parent: control
        visible: control.redesignBlock && MainCtx.redesignPresentation.preview && hoverHandler.hovered && MainCtx.redesignSeekPreviewUrl.toString().length > 0
        focus: false; modal: false; closePolicy: C.Popup.NoAutoClose
        width: 224 * VLCStyle.scale; height: 148 * VLCStyle.scale; padding: 8 * VLCStyle.scale
        x: Helpers.clamp(control.previewPosition * control.width - width / 2, 0, Math.max(0, control.width - width))
        y: -height - 12 * VLCStyle.scale
        background: Rectangle { color: theme.bg.primary; radius: 6 * VLCStyle.scale; border.color: theme.border }
        contentItem: Image { source: MainCtx.redesignSeekPreviewUrl; fillMode: Image.PreserveAspectFit }
    }

    Connections {
        target:MainCtx
        function onRedesignPresentationChanged(){if(MainCtx.redesignPresentation.cues)MainCtx.requestRedesignSubtitleTimeline()}
    }
    Connections {
        target:Player
        function onInputChanged(){if(MainCtx.redesignPresentation.cues)MainCtx.requestRedesignSubtitleTimeline()}
    }
    Timer {id:refreshCueTracks;interval:180;onTriggered:if(MainCtx.redesignPresentation.cues)MainCtx.requestRedesignSubtitleTimeline()}
    Connections {
        target:Player.subtitleTracks
        function onDataChanged(){refreshCueTracks.restart()}
        function onModelReset(){refreshCueTracks.restart()}
        function onRowsInserted(){refreshCueTracks.restart()}
    }
    Item {
        id:cueStrip;objectName:"redesignSubtitleCueStrip"
        visible:control.redesignBlock&&MainCtx.redesignPresentation.cues
        x:0;y:-40*VLCStyle.scale;width:control.width;height:32*VLCStyle.scale;clip:true
        readonly property real duration:Math.max(1,Player.length.toSeconds())
        readonly property var cues:MainCtx.redesignTimelineCues
        Rectangle {anchors.fill:parent;color:theme.bg.secondary;radius:4*VLCStyle.scale}
        Repeater {
            model:cueStrip.visible?cueStrip.cues:[]
            Rectangle {
                id:cueBlock;objectName:"redesignSubtitleCueBlock"
                required property var modelData
                readonly property real begin:{const delay=Player.subtitleDelayMS,fps=Player.subtitleFPS;return MainCtx.redesignSubtitleCueTime(modelData.start)}
                readonly property real end:{const delay=Player.subtitleDelayMS,fps=Player.subtitleFPS;return MainCtx.redesignSubtitleCueTime(modelData.end)}
                function moveCue(milliseconds){Player.subtitleDelayMS=Math.round(milliseconds)}
                property real baseDelay:0
                x:begin/cueStrip.duration*cueStrip.width
                width:Math.max(3*VLCStyle.scale,(end-begin)/cueStrip.duration*cueStrip.width)
                height:cueStrip.height-4*VLCStyle.scale;y:2*VLCStyle.scale;radius:3*VLCStyle.scale
                color:drag.active?theme.accent:Qt.darker(theme.accent,1.8)
                border.color:theme.accent
                Accessible.name:modelData.text;Accessible.role:Accessible.Button
                Text {anchors.fill:parent;anchors.margins:4*VLCStyle.scale;text:cueBlock.modelData.text;textFormat:Text.PlainText;elide:Text.ElideRight;color:theme.fg.primary;font.pixelSize:11*VLCStyle.scale;verticalAlignment:Text.AlignVCenter}
                C.ToolTip.visible:cueHover.hovered;C.ToolTip.text:modelData.text+" · "+begin.toFixed(3)+" s";HoverHandler {id:cueHover}
                TapHandler {onTapped:Player.position=Math.max(0,Math.min(1,cueBlock.begin/cueStrip.duration))}
                DragHandler {
                    id:drag;target:null;yAxis.enabled:false
                    onActiveChanged:{if(active)cueBlock.baseDelay=Player.subtitleDelayMS;else if(translation.x!==0)MainCtx.setSettingValue("Redesign/subtitle-delay-ms/"+encodeURIComponent(Player.url.toString()),Player.subtitleDelayMS)}
                    onTranslationChanged:if(active)cueBlock.moveCue(cueBlock.baseDelay+translation.x/Math.max(1,cueStrip.width)*cueStrip.duration*1000)
                    onCanceled:{cueBlock.moveCue(cueBlock.baseDelay);MainCtx.setSettingValue("Redesign/subtitle-delay-ms/"+encodeURIComponent(Player.url.toString()),cueBlock.baseDelay)}
                }
            }
        }
        Rectangle {x:Player.position*parent.width;width:2*VLCStyle.scale;height:parent.height;color:theme.fg.primary}
        Text {visible:cueStrip.cues.length===0;anchors.centerIn:parent;text:MainCtx.redesignTimelineBusy?qsTr("Loading subtitle timeline…"):qsTr("No readable subtitle cues in this track");color:theme.fg.secondary;font.pixelSize:11*VLCStyle.scale}
    }

    property bool _currentChapterHovered: false
    property real _tooltipPosition: (Widgets.PointingToolTipAttached.instance?.pos.x ?? 0.0) / width

    property color backgroundColor: theme.bg.primary
    property real touchHandlerMargin: VLCStyle.touchHandlerMargin

    Keys.onRightPressed: Player.jumpFwd()
    Keys.onLeftPressed: Player.jumpBwd()

    Accessible.onIncreaseAction: Player.jumpFwd()
    Accessible.onDecreaseAction: Player.jumpBwd()

    function showChapterMarks() {
        _isSeekPointsShown = true
        seekpointTimer.restart()
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Slider

        enabled: control.enabled
        focused: control.visualFocus
        hovered: hoverHandler.hovered
    }

    Timer {
        id: seekpointTimer
        running: Player.hasChapters && !hoverHandler.hovered && _isSeekPointsShown
        interval: 3000
        onTriggered: control._isSeekPointsShown = false
    }

    Widgets.PointingToolTipAttached.visible: (hoverHandler.hovered || control.visualFocus || dragHandler.active)

    Widgets.PointingToolTipAttached.text: {
        if (!Widgets.PointingToolTipAttached.instance?.visible)
            return ""

        let _text

        const length = Player.length
        if (hoverHandler.hovered)
            _text = length.scale(Widgets.PointingToolTipAttached.instance.pos.x / control.width)
        else
            _text = Player.time

        _text = _text.formatHMS(length.isSubSecond() ? VLCTick.SubSecondFormattedAsMS : 0)

        if (Player.hasChapters)
            _text += " - " + Player.chapters.getNameAtPosition(control._tooltipPosition)

        return _text
    }

    Widgets.PointingToolTipAttached.pos: Qt.point(hoverHandler.hovered ? Helpers.clamp(hoverHandler.point.position.x, 0, control.availableWidth)
                                                                       : (control.visualPosition * control.width), 0)

    FSM {
        id: fsm
        signal playerUpdatePosition(real position)
        signal pressControl(real position, bool forcePrecise)
        signal releaseControl(real position, bool forcePrecise)
        signal moveControl(real position, bool forcePrecise)
        signal inputChanged()

        //each signal is associated to a key, when a signal is received,
        //transitions of active state for the given key are evaluated
        signalMap: ({
            "playerUpdatePosition": fsm.playerUpdatePosition,
            "pressControl": fsm.pressControl,
            "releaseControl": fsm.releaseControl,
            "moveControl": fsm.moveControl,
            "inputChanged": fsm.inputChanged
        })

        initialState: fsmReleased

        function _setPositionFromValue(position) {
            control.value = position
        }

        function _seekToPosition(position, threshold, forcePrecise) {
            position = Helpers.clamp(position, 0., 1.)
            control.value = position
            if (!forcePrecise) {
                const chapter = Player.chapters.getClosestChapterFromPos(position, threshold)
                if (chapter !== -1) {
                    Player.chapters.selectChapter(chapter)
                    return
                }
            }
            Player.position = position
        }

        FSMState {
            id: fsmReleased

            transitions: ({
                "playerUpdatePosition": {
                    action: (position) => {
                        control.value = position
                    }
                },
                "pressControl": {
                    action: (position, forcePrecise) => {
                        control.forceActiveFocus()
                        fsm._seekToPosition(position, VLCStyle.dp(4, VLCStyle.scale) / control.width, forcePrecise)
                    },
                    target: fsmHeld
                }
            })
        }

        FSMState  {
            id: fsmHeld

            transitions: ({
                "moveControl": {
                    action: (position, forcePrecise) => {
                        fsm._seekToPosition(position, VLCStyle.dp(2, VLCStyle.scale) / control.width, forcePrecise)
                    }
                },
                "releaseControl": {
                    target: fsmReleased
                },
                "inputChanged": {
                    target: fsmHeldWrongInput
                }
            })
        }

        FSMState  {
            id: fsmHeldWrongInput

            function enter() {
                fsm._setPositionFromValue(Player.position)
            }

            transitions: ({
                "playerUpdatePosition": {
                    action: fsm._setPositionFromValue
                },
                "releaseControl": {
                    target: fsmReleased
                }
            })
        }
    }

    Connections {
        target: Player
        function onPositionChanged() {  fsm.playerUpdatePosition(Player.position) }
        function onInputChanged() {  fsm.inputChanged() }
    }

    Component.onCompleted: value = Player.position

    implicitHeight: control.barHeight
    height: implicitHeight

    padding: 0

    //we use our own HoverHandler
    hoverEnabled: false

    HoverHandler {
        id: hoverHandler

        onHoveredChanged: () => {
            if (hovered) {
                if(Player.hasChapters)
                    control._isSeekPointsShown = true
            } else {
                if(Player.hasChapters)
                    seekpointTimer.restart()
            }
        }
    }

    contentItem: Item {
        implicitHeight: control.implicitHeight
        implicitWidth: control.implicitWidth

        //placing the TapHandler directly in the Control doesn't work with 6.2
        TapHandler {
            acceptedButtons: Qt.LeftButton

            gesturePolicy: TapHandler.WithinBounds

            // Make it easier to tap with touch screen:
            margin: MainCtx.usingTouch ? control.touchHandlerMargin : 0.0

            onPressedChanged: {
                if (pressed) {
                    fsm.pressControl(point.position.x / control.width, point.modifiers === Qt.ShiftModifier)
                } else {
                    // NOTE: Point is still valid at this point.
                    fsm.releaseControl(point.position.x / control.width, point.modifiers === Qt.ShiftModifier)
                }
            }
        }

        DragHandler {
            id: dragHandler
            acceptedButtons: Qt.LeftButton

            // Make it easier to drag with touch screen:
            margin: MainCtx.usingTouch ? control.touchHandlerMargin : 0

            target: null
            dragThreshold: 0
            grabPermissions: PointerHandler.CanTakeOverFromAnything

            property bool filterEvents: false

            function moveControl() {
                fsm.moveControl(dragHandler.centroid.position.x / control.width,
                                dragHandler.centroid.modifiers === Qt.ShiftModifier)
            }

            onActiveChanged: {
                if (active) {
                    filterEvents = false
                    fsm.pressControl(centroid.position.x / control.width, centroid.modifiers === Qt.ShiftModifier)
                } else {
                    fsm.releaseControl( centroid.position.x / control.width, centroid.modifiers === Qt.ShiftModifier)
                }
            }

            onCentroidChanged: {
                // FIXME: Qt 6.5 use xAxis.onActiveValueChanged in the DragHandler
                if (filterEvents) {
                    Qt.callLater(dragHandler.moveControl)
                } else {
                    dragHandler.moveControl()
                    filterEvents = true
                }
            }
        }

        WheelHandler {
            orientation: Qt.Vertical | Qt.Horizontal

            acceptedDevices: PointerDevice.AllDevices

            onWheel: (wheel) => {
                wheelToVLC.qmlWheelEvent(wheel)
                wheel.accepted = true
            }
        }

        WheelToVLCConverter {
            id: wheelToVLC

            function handleVertical(steps: int) {
                if (steps > 0)
                    Player.jumpFwd()
                else
                    Player.jumpBwd()
            }

            function handleHorizontal(steps: int) {
                if (steps > 0)
                    Player.jumpBwd()
                else
                    Player.jumpFwd()
            }

            Component.onCompleted: {
                wheelUpDown.connect(wheelToVLC, wheelToVLC.handleVertical)
                wheelLeftRight.connect(wheelToVLC, wheelToVLC.handleHorizontal)
            }
        }
    }

    background: Item {
        width: control.availableWidth
        implicitHeight: control.implicitHeight
        height: implicitHeight

        Rectangle {
            id: sliderRect
            visible: !Player.hasChapters
            color: control.backgroundColor
            anchors.fill: parent
            radius: control.redesignBlock ? VLCStyle.dp(6, VLCStyle.scale) : implicitHeight
        }

        Repeater {
            id: seekpointsRptr

            anchors.left: parent.left
            anchors.right: parent.right
            height: control.barHeight
            visible: Player.hasChapters

            model: Player.chapters
            Item {
                Rectangle {
                    id: seekpointsRect
                    readonly property real startPosition: model.startPosition ??  0.0
                    readonly property real endPosition: model.endPosition ?? 1.0

                    readonly property int _currentChapter: {
                        if (control.visualPosition < seekpointsRect.startPosition)
                            return 1
                        else if (control.visualPosition > seekpointsRect.endPosition)
                            return -1
                        return 0
                    }
                    on_CurrentChapterChanged: {
                        if(_hovered)
                            control._currentChapterHovered = _currentChapter === 0
                    }

                    readonly property bool _hovered: hoverHandler.hovered &&
                                            (hoverHandler.point.position.x > x && hoverHandler.point.position.x < x+width)

                    color: _currentChapter < 0 ? theme.fg.primary : control.backgroundColor
                    width: sliderRect.width * seekpointsRect.endPosition - x - control._seekPointsDistance
                    x: sliderRect.width * seekpointsRect.startPosition

                    Rectangle {
                        id: progressRepRect
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        radius: parent.radius

                        width: sliderRect.width * control.visualPosition - parent.x - control._seekPointsDistance
                        visible: parent._currentChapter === 0
                        color: theme.fg.primary

                        // We use dynamic mutability group because time interpolation
                        // may induce rapid change:
                        // TODO: Do not use `Binding` when minimum Qt is 6.12:
                        Binding {
                            target: progressRepRect
                            property: "mutabilityGroup"
                            when: (progressRepRect.mutabilityGroup !== undefined)
                            value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.DynamicMutabilityGroup
                                                                                          : Item.StaticMutabilityGroup
                        }
                    }
                }

                transitions: [
                    Transition {
                        to: "*"
                        SequentialAnimation{
                            PropertyAction { targets: control; property: "_currentChapterHovered" }
                            NumberAnimation {
                                targets: [seekpointsRect, progressRepRect]; properties: "height, y, radius"
                                duration: VLCStyle.duration_short; easing.type: Easing.InSine
                            }
                        }
                    }
                ]

                states:[
                    State {
                        name: "visible"
                        PropertyChanges {
                            target: control;
                            _currentChapterHovered: seekpointsRect._currentChapter === 0 ? false : control._currentChapterHovered
                        }
                        PropertyChanges { target: seekpointsRect; height: control.barHeight }
                        PropertyChanges { target: seekpointsRect; y: 0 }
                        PropertyChanges { target: seekpointsRect; radius: control._seekPointsRadius }
                    },
                    State {
                        name: "visibleLarge"
                        PropertyChanges {
                            target: control;
                            _currentChapterHovered: seekpointsRect._currentChapter === 0 ? true : control._currentChapterHovered
                        }
                        PropertyChanges { target: seekpointsRect; height: control._scaledBarHeight }
                        PropertyChanges { target: seekpointsRect; y: control._scaledY }
                        PropertyChanges { target: seekpointsRect; radius: control._scaledSeekPointsRadius }
                    }
                ]

                state: (seekpointsRect._hovered || (seekpointsRect._currentChapter === 0 && fsm._state === fsm.fsmHeld))
                       ? "visibleLarge"
                       : "visible"
            }
        }

        Rectangle {
            id: progressRect
            x: control.redesignBlock ? 2 : 0
            y: control.redesignBlock ? 2 : 0
            width: control.visualPosition * (parent.width - (control.redesignBlock ? 4 : 0))
            visible: !Player.hasChapters
            color: control.redesignBlock ? theme.accent : theme.fg.primary
            opacity: control.redesignBlock ? 0.18 : 1
            height: control.barHeight - (control.redesignBlock ? 4 : 0)
            radius: control.redesignBlock ? VLCStyle.dp(4, VLCStyle.scale) : control._seekPointsRadius

            // We use dynamic mutability group because time interpolation
            // may induce rapid change:
            // TODO: Do not use `Binding` when minimum Qt is 6.12:
            Binding {
                target: progressRect
                property: "mutabilityGroup"
                when: (progressRect.mutabilityGroup !== undefined)
                value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.DynamicMutabilityGroup
                                                                              : Item.StaticMutabilityGroup
            }
        }

        Item {
            id: waveformBars
            anchors.fill: parent
            visible: control.waveformPeaks.length > 0
            Repeater {
                model: Math.floor(waveformBars.width / 3)
                Rectangle {
                    required property int index
                    readonly property real peak: {
                        const values = control.waveformPeaks
                        const count = Math.max(1, Math.floor(waveformBars.width / 3))
                        const begin = Math.floor(index * values.length / count)
                        const end = Math.max(begin + 1, Math.floor((index + 1) * values.length / count))
                        let value = 0
                        for (let sample = begin; sample < end; ++sample) value = Math.max(value, values[sample] ?? 0)
                        return value
                    }
                    x: index * 3 + 1
                    width: 1
                    height: Math.max(1, peak * (waveformBars.height - 6))
                    y: (waveformBars.height - height) / 2
                    color: x <= control.visualPosition * waveformBars.width ? theme.accent : "#6b6b6b"
                }
            }
        }

        Rectangle {
            id: bufferRect
            property int bufferAnimWidth: VLCStyle.dp(100, VLCStyle.scale)
            property int bufferAnimPosition: 0
            property int bufferFrames: 1000
            property alias animateLoading: loadingAnim.running
            property bool display: false
            property bool buffering: Player.buffering > 0 && Player.buffering < 1

            height: control.barHeight
            opacity: 0.4
            color: theme.fg.neutral //FIXME buffer color ?
            radius: control.barHeight

            // TODO: Do not use `Binding` when minimum Qt is 6.12:
            Binding {
                target: bufferRect
                property: "mutabilityGroup"
                when: bufferRect.mutabilityGroup !== undefined
                value: bufferRect.buffering ? Item.DynamicMutabilityGroup
                                            : Item.StaticMutabilityGroup
            }

            Timer {
                id: bufferingTimer
                interval: VLCStyle.duration_humanMoment
                repeat: false
                running: false
                onTriggered: {
                    bufferRect.display = true
                }
            }

            onBufferingChanged: {
                if(buffering)
                {
                    bufferingTimer.start()
                }
                else
                {
                    bufferingTimer.stop()
                    bufferRect.display = false
                }
            }

            states: [
                State {
                    name: "hidden"
                    when: !control.visible
                    PropertyChanges {
                        target: bufferRect
                        width: bufferAnimWidth
                        visible: false
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "buffering not started"
                    when: control.visible && Player.buffering === 0
                    PropertyChanges {
                        target: bufferRect
                        width: bufferAnimWidth
                        visible: true
                        x: (bufferAnimPosition / bufferFrames) * (parent.width - bufferAnimWidth)
                        animateLoading: true
                    }
                },
                State {
                    name: "time to start playing known"
                    when: control.visible && bufferRect.display
                    PropertyChanges {
                        target: bufferRect
                        width: Player.buffering * parent.width
                        visible: true
                        x: 0
                        animateLoading: false
                    }
                },
                State {
                    name: "playing from buffer"
                    when: control.visible && !bufferRect.display
                    PropertyChanges {
                        target: bufferRect
                        width: Player.buffering * parent.width
                        visible: false
                        x: 0
                        animateLoading: false
                    }
                }
            ]

            SequentialAnimation on bufferAnimPosition {
                id: loadingAnim
                running: bufferRect.animateLoading
                loops: Animation.Infinite
                PropertyAnimation {
                    from: 0.0
                    to: bufferRect.bufferFrames
                    duration: VLCStyle.durationSliderBouncing
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: VLCStyle.duration_veryLong
                }
                PropertyAnimation {
                    from: bufferRect.bufferFrames
                    to: 0.0
                    duration: VLCStyle.durationSliderBouncing
                    easing.type: "OutBounce"
                }
                PauseAnimation {
                    duration: VLCStyle.duration_veryLong
                }
            }
        }
    }

    Rectangle {
        id: sliderHandle

        property int _size: control.redesignBlock ? VLCStyle.dp(8, VLCStyle.scale) : control.barHeight * 3

        x: (control.visualPosition * control.availableWidth) - width / 2
        y: (control.barHeight - height) / 2

        implicitWidth: sliderHandle._size
        implicitHeight: control.redesignBlock ? control.barHeight - 4 : sliderHandle._size
        radius: control.redesignBlock ? VLCStyle.dp(4, VLCStyle.scale) : VLCStyle.margin_small
        color: theme.fg.primary

        // We use dynamic mutability group because time interpolation
        // may induce rapid change:
        // TODO: Do not use `Binding` when minimum Qt is 6.12:
        Binding {
            target: sliderHandle
            property: "mutabilityGroup"
            when: (sliderHandle.mutabilityGroup !== undefined)
            value: (Player.playingState === Player.PLAYING_STATE_PLAYING) ? Item.DynamicMutabilityGroup
                                                                          : Item.StaticMutabilityGroup
        }

        transitions: [
            Transition {
                to: "hidden"
                SequentialAnimation {
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"
                        to: 0
                        duration: VLCStyle.duration_short; easing.type: Easing.OutSine
                    }
                    PropertyAction { target: sliderHandle; property: "visible"; value: false; }
                }
            },
            Transition {
                to: "visible"
                SequentialAnimation {
                    PropertyAction { target: sliderHandle; property: "visible"; value: true; }
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"
                        to: sliderHandle._size
                        duration: VLCStyle.duration_short; easing.type: Easing.InSine
                    }
                }
            },
            Transition {
                to: "visibleLarge"
                SequentialAnimation {
                    PropertyAction { target: sliderHandle; property: "visible"; value: true; }
                    NumberAnimation {
                        target: sliderHandle; properties: "implicitWidth,implicitHeight"
                        to: sliderHandle._size * (0.8 * control._hoveredScalingFactor)
                        duration: VLCStyle.duration_short; easing.type: Easing.InSine
                    }
                }
            }
        ]

        state: control.redesignBlock ? "block" : (hoverHandler.hovered || control.visualFocus)
               ? ((control._currentChapterHovered || (Player.hasChapters && fsm._state === fsm.fsmHeld)) ? "visibleLarge" : "visible")
               : "hidden"
    }
}
