import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design

// The line above a terminal: its kind, name and place, uncommitted changes, its
// own colors, the agent state, and for the focused pane its width, full-width
// and close actions, drawn while the pointer is over the header. Presentation only; the pane passes facts in and handles
// the signals. The gallery shows it with sample facts.
Item {
    id: header
    property var tones
    property string terminalId
    property string icon: "shell"
    property bool stopped: false        // an app that is not running
    property string name
    property string place
    property bool dirty: false
    property int ownTheme: -1           // the terminal's own palette, or -1
    property string agentState          // empty for plain shells
    property bool needsInput: false
    property bool focused: false        // shows the pane actions
    property bool fullWidth: false
    property bool exited: false
    property bool busy: false
    property bool menuEnabled: true
    property bool colorsEnabled: true
    signal widthClicked()
    signal fullClicked()
    signal closeClicked()
    signal colorsClicked()
    signal menuClicked(Item anchor)
    height: 32
    HoverHandler { id: headerHover }
    property bool actionsShown: headerHover.hovered   // the gallery pins it on

    // Room for the identity left of the state, colors swatch and menu.
    readonly property real room: width - 51 - (paneActions.visible ? paneActions.width + 4 : 0) - headerState.width - (headerState.width > 0 ? 12 : 0) - (ownColors.visible ? 22 : 0) - (dirtyMark.visible ? 14 : 0)
    ChromeIcon {
        x: 12; y: 9; width: 15; height: 15; name: header.icon
        color: header.stopped ? header.tones.waiting : header.tones.muted
    }
    Text {
        id: paneName; objectName: "paneName"; x: 33; y: 10; visible: text.length > 0
        width: Math.min(implicitWidth, Math.max(0, (header.room - x) * 0.5)); elide: Text.ElideRight
        text: header.name; textFormat: Text.PlainText; font.pixelSize: Design.textSmall; font.weight: Font.DemiBold
        color: header.tones.foreground
    }
    Text {
        id: paneTitle; objectName: "paneTitle"; x: paneName.visible ? paneName.x + paneName.width + 7 : 33; y: 10
        width: Math.min(implicitWidth, Math.max(0, header.room - x)); elide: Text.ElideMiddle
        text: header.place; textFormat: Text.PlainText; font.pixelSize: Design.textSmall
        color: paneName.visible ? header.tones.muted : header.tones.foreground
    }
    // Uncommitted changes in this pane's Git worktree.
    Rectangle {
        id: dirtyMark; objectName: "paneDirty"; visible: header.dirty
        x: paneTitle.x + paneTitle.width + 7; y: 15; width: 6; height: 6; radius: 3
        color: header.tones.accent
        Accessible.role: Accessible.StaticText; Accessible.name: "Uncommitted changes"
        MouseArea { id: dirtyArea; anchors.fill: parent; anchors.margins: -5; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        ToolTip.visible: dirtyArea.containsMouse; ToolTip.delay: 400; ToolTip.text: "Uncommitted changes"
    }
    // A swatch of the terminal's own palette; the frame keeps workspace colors.
    Rectangle {
        id: ownColors; objectName: "terminalOwnColors" + header.terminalId
        visible: header.ownTheme >= 0
        x: (dirtyMark.visible ? dirtyMark.x + dirtyMark.width : paneTitle.x + paneTitle.width) + 8; y: 12; width: 12; height: 12; radius: 6
        color: visible ? studyThemes.theme(header.ownTheme).background : "transparent"
        border.width: Design.lineStrong; border.color: visible ? studyThemes.theme(header.ownTheme).accent : "transparent"
        Accessible.role: Accessible.Button; Accessible.name: "Terminal colors: " + (visible ? studyThemes.theme(header.ownTheme).name : "")
        MouseArea {
            id: ownColorsArea; anchors.fill: parent; anchors.margins: -4; hoverEnabled: true; enabled: header.colorsEnabled
            cursorShape: Qt.PointingHandCursor; onClicked: header.colorsClicked()
        }
        ToolTip.visible: ownColorsArea.containsMouse; ToolTip.delay: 400
        ToolTip.text: "Own colors: " + (visible ? studyThemes.theme(header.ownTheme).name : "") + " · click to change"
    }
    Text {
        id: headerState; x: (paneActions.visible ? paneActions.x - 8 : parent.width - 36) - width; y: 10; width: Math.min(implicitWidth, parent.width * 0.45)
        text: header.agentState; textFormat: Text.PlainText; elide: Text.ElideMiddle; font.pixelSize: Design.textSmall
        color: header.needsInput ? header.tones.waiting : header.agentState === "Ready" ? header.tones.ready : header.tones.muted
    }
    // The focused pane's own actions, shown on hover; their room stays reserved
    // so the title does not move. They leave keyboard focus in the terminal.
    Row {
        id: paneActions; objectName: "paneActions"; x: parent.width - 33 - width; y: 0; spacing: Design.space2xs; visible: header.focused
        opacity: header.actionsShown ? 1 : 0
        ChromeButton { objectName: "paneWidthButton"; tones: header.tones; height: 28; focusPolicy: Qt.NoFocus; iconName: "width"; hint: "Cycle width"; keys: studyShortcuts.labels["cycle-width"] || ""; onClicked: header.widthClicked() }
        ChromeButton {
            objectName: "paneFullButton"; tones: header.tones; height: 28; focusPolicy: Qt.NoFocus; iconName: "full"; selected: header.fullWidth
            hint: header.fullWidth ? "Restore width" : "Full width"; keys: studyShortcuts.labels["full-width"] || ""; onClicked: header.fullClicked()
        }
        ChromeButton {
            objectName: "paneCloseButton"; tones: header.tones; height: 28; focusPolicy: Qt.NoFocus; iconName: "close"; enabled: !header.busy
            hint: header.exited ? "Close pane" : "Close terminal…"; keys: studyShortcuts.labels["close-terminal"] || ""; onClicked: header.closeClicked()
        }
    }
    ChromeButton {
        objectName: "paneLayoutButton"; tones: header.tones; anchors.right: parent.right; anchors.rightMargin: Design.spaceXs; y: 0
        iconName: "more"; height: 28; hint: "Terminal layout"; enabled: header.menuEnabled; onClicked: header.menuClicked(this)
    }
}
