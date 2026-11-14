import QtQuick
import QtQuick.Controls
import Band.Design

// The top bar: the workspace name, one marker per terminal in the strip (a
// small map of it: each marker as wide as its pane's share of the window, in
// its state's color), the focused terminal's position, New terminal and Overview. Presentation only;
// the window passes the markers in and handles the signals.
Item {
    id: topbar
    property var tones
    property color accent: tones.accent
    property string workspaceName
    property bool overview: false
    property bool canCreate: true
    // One entry per terminal in the workspace: {title, selected, waiting, ready,
    // share}; share is the pane's width as a part of the window, 0 to 1.
    property var markers: []
    // Whether the terminal at a position is on screen; read per marker, so
    // scrolling the strip does not rebuild the list.
    property var inView: position => true
    property int focusPosition: -1      // the focused terminal's position in markers, or -1
    signal workspaceClicked()
    signal markerClicked(int position)
    signal newTerminalClicked()
    signal overviewClicked()
    height: 32

    readonly property int markerCount: markers.length
    // Markers that fit between the side controls; past that, a window follows the selection.
    // The row is centred: half its width must clear the workspace name on the left and
    // the count, New terminal and Overview on the right, with 8 px to spare on each side.
    readonly property real markerRoom: Math.min(width / 2 - workspaceButton.width, width / 2 - focusedCount.width - newTerminalButton.width - overviewButton.width - 36) - 8
    readonly property int markerSlots: Math.max(1, Math.min(markerCount, Math.floor(2 * markerRoom / 28)))
    readonly property int markerStart: Math.max(0, Math.min(markerCount - markerSlots, Math.max(0, focusPosition) - Math.floor(markerSlots / 2)))
    readonly property bool showMarkers: !overview && markerCount > 0

    ChromeButton { id: workspaceButton; objectName: "workspaceButton"; tones: topbar.tones; leftAligned: true; width: Math.min(200, Math.max(80, topbar.width / 4)); text: topbar.workspaceName; hint: "Choose workspace"; onClicked: topbar.workspaceClicked() }
    Rectangle {
        anchors.centerIn: parent; width: indicatorRow.width + 8; height: 28; radius: 14; color: topbar.tones.background
        visible: topbar.showMarkers
    }
    Row {
        id: indicatorRow; objectName: "indicatorRow"; anchors.centerIn: parent; spacing: 0; visible: topbar.showMarkers
        Repeater {
            model: topbar.markerSlots
            ChromeButton {
                required property int index
                readonly property int position: topbar.markerStart + index
                readonly property var marker: topbar.markers[position] || ({title: "Terminal", selected: false, waiting: false})
                readonly property real share: Math.max(0, Math.min(1, marker.share === undefined ? 0.5 : marker.share))
                tones: topbar.tones; implicitWidth: 28; implicitHeight: 28; text: ""; hint: marker.title || "Terminal"
                Rectangle {
                    // Width is the pane's share (a third 13, a half 16, full 24); the
                    // focused one is the accent and a little taller. A pane that needs
                    // input or whose agent is ready shows that color, on screen or not;
                    // the rest are muted on screen and fainter off it.
                    anchors.centerIn: parent; width: Math.round(8 + 16 * parent.share); height: parent.marker.selected ? 8 : 6; radius: height / 2
                    color: parent.marker.selected ? topbar.accent : parent.marker.waiting ? topbar.tones.waiting : parent.marker.ready ? topbar.tones.ready
                        : topbar.inView(parent.position) ? topbar.tones.muted : topbar.tones.border
                    Behavior on width { NumberAnimation { duration: Design.fade; easing.type: Easing.OutCubic } }
                    Behavior on height { NumberAnimation { duration: Design.fade; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Design.fade } }
                }
                Rectangle {
                    x: parent.width - 5; y: 1; width: 4; height: 4; radius: 2; color: topbar.tones.waiting
                    // The focused marker is the accent, so its need for input is a dot.
                    visible: !!parent.marker.waiting && !!parent.marker.selected
                }
                onClicked: topbar.markerClicked(position)
            }
        }
    }
    Label {
        id: focusedCount; objectName: "focusedCount"
        anchors.left: indicatorRow.right; anchors.leftMargin: Design.spaceL; anchors.verticalCenter: parent.verticalCenter
        visible: topbar.showMarkers
        text: Math.max(0, topbar.focusPosition + 1) + " / " + topbar.markerCount
        color: topbar.tones.muted; font.pixelSize: Design.textSmall
    }
    ChromeButton {
        id: newTerminalButton; objectName: "newTerminalButton"; tones: topbar.tones; iconName: "plus"; hint: "New terminal"; keys: studyShortcuts.labels["new-terminal"] || ""; enabled: topbar.canCreate
        x: topbar.overview ? overviewButton.x - width - 12 : focusedCount.x + focusedCount.width + 12
        onClicked: topbar.newTerminalClicked()
    }
    ChromeButton { id: overviewButton; objectName: "overviewButton"; tones: topbar.tones; quiet: true; anchors.right: parent.right; text: topbar.overview ? "Done" : "Overview"; keys: studyShortcuts.labels["overview"] || ""; onClicked: topbar.overviewClicked() }
}
