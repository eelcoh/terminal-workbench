import QtQuick
import QtQuick.Controls
import Band.Design

// The top bar: the workspace name, one marker per terminal in the strip, the
// focused terminal's position, New terminal and Overview. Presentation only;
// the window passes the markers in and handles the signals.
Item {
    id: topbar
    property var tones
    property color accent: tones.accent
    property string workspaceName
    property bool overview: false
    property bool canCreate: true
    // One entry per terminal in the workspace: {title, selected, waiting}.
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
                tones: topbar.tones; implicitWidth: 28; implicitHeight: 28; text: ""; hint: marker.title || "Terminal"
                Rectangle {
                    anchors.centerIn: parent; width: parent.marker.selected ? 24 : 14; height: 8; radius: 4
                    color: parent.marker.selected ? topbar.accent : topbar.inView(parent.position) ? topbar.tones.muted : topbar.tones.border
                    Behavior on width { NumberAnimation { duration: Design.fade; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Design.fade } }
                }
                Rectangle {
                    x: parent.width - 5; y: 1; width: 4; height: 4; radius: 2; color: topbar.tones.waiting
                    visible: !!parent.marker.waiting
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
