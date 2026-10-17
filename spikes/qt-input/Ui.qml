import QtQuick
import QtQuick.Controls
import Workbench.Probe
ApplicationWindow {
    id: window
    width: 1240; height: 760; minimumWidth: 680; minimumHeight: 440
    visible: true; title: "Terminal Workbench · Qt UI study"; color: "#101318"
    property int workspace: 0
    property int selected: 0
    property bool overview: false
    property bool reducedMotion: false
    property var widths: [0.5, 0.5, 0.5, 0.5, 0.5, 0.5]
    property var fullWidths: [false, false, false, false, false, false]
    property var remembered: [0, 3, -1]
    property var positions: [0, 0, 0]
    readonly property var spaces: ["Workbench", "Side project", "Empty workspace"]
    readonly property string chord: Qt.platform.os === "osx" ? "Ctrl+Alt+" : "Meta+Alt+"
    readonly property string chordLabel: Qt.platform.os === "osx" ? "⌘⌥" : "Super+Alt+"
    readonly property color accent: "#b4c9ec"
    readonly property int motion: reducedMotion ? 0 : 220
    function panel(i) { return panels.itemAt(i) }
    function paneWidth(i) { return Math.max(240, (deck.width + 12) * (fullWidths[i] ? 1 : widths[i]) - 12) }
    function left(i) { let x = 0; for (let j = Math.floor(i / 3) * 3; j < i; ++j) x += paneWidth(j) + 12; return x }
    function stripWidth() { return workspace === 2 ? deck.width : left(workspace * 3 + 2) + paneWidth(workspace * 3 + 2) }
    function reveal() {
        if (overview || selected < 0) return
        let x = left(selected), w = paneWidth(selected)
        if (x < deck.contentX) deck.contentX = x
        else if (x + w > deck.contentX + deck.width) deck.contentX = Math.max(0, x + w - deck.width)
    }
    function focusPane(i) {
        if (i < 0 || i >= 6) return
        selected = i; workspace = Math.floor(i / 3)
        let values = remembered.slice(); values[workspace] = i; remembered = values
        if (!overview) { reveal(); panel(i).terminal.forceActiveFocus() }
    }
    function chooseWorkspace(i) {
        let values = positions.slice(); values[workspace] = deck.contentX; positions = values
        workspace = Math.max(0, Math.min(2, i)); selected = remembered[workspace]
        drawer.close(); deck.contentX = overview ? 0 : positions[workspace]
        if (!overview && selected >= 0) Qt.callLater(function() { reveal(); panel(selected).terminal.forceActiveFocus() })
        else window.contentItem.forceActiveFocus()
    }
    function toggleOverview() {
        if (!overview) { let values = positions.slice(); values[workspace] = deck.contentX; positions = values }
        overview = !overview; deck.contentX = overview ? 0 : positions[workspace]; deck.contentY = 0
        if (overview) window.contentItem.forceActiveFocus()
        else if (selected >= 0) Qt.callLater(function() { reveal(); panel(selected).terminal.forceActiveFocus() })
    }
    function enterPane(i) { focusPane(i); if (overview) toggleOverview() }
    function navigate(delta) {
        if (selected >= 0) { focusPane(Math.max(workspace * 3, Math.min(workspace * 3 + 2, selected + delta))) }
    }
    function cycleWidth() {
        if (selected < 0) return
        let values = widths.slice(), full = fullWidths.slice()
        values[selected] = values[selected] === 0.5 ? 2/3 : values[selected] > 0.6 ? 1/3 : 0.5
        full[selected] = false; widths = values; fullWidths = full; Qt.callLater(reveal)
    }
    function toggleFull() {
        if (selected < 0) return
        let values = fullWidths.slice(); values[selected] = !values[selected]; fullWidths = values; Qt.callLater(reveal)
    }
    component QuietButton: Button {
        id: control
        property string hint: text
        implicitWidth: Math.max(36, contentItem.implicitWidth + 22); implicitHeight: 36
        hoverEnabled: true; focusPolicy: Qt.StrongFocus
        Accessible.name: hint
        contentItem: Text { text: control.text; color: control.down ? "#ffffff" : "#c2c9d4"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 7; color: control.down ? "#323b4b" : control.hovered || control.visualFocus ? "#242c38" : "transparent"; border.width: control.visualFocus ? 1 : 0; border.color: window.accent }
        ToolTip.visible: hovered; ToolTip.delay: 650; ToolTip.text: hint
    }
    Rectangle { x: 48; y: 0; width: 1; height: parent.height; color: "#232933" }
    Column {
        x: 6; y: 10; spacing: 8
        QuietButton { text: "▤"; hint: "Workspaces"; onClicked: drawer.open() }
        QuietButton { text: "▦"; hint: "Overview · " + window.chordLabel + "O"; onClicked: window.toggleOverview() }
    }
    QuietButton { x: 6; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; text: "?"; hint: "Keyboard shortcuts"; onClicked: help.open() }
    Item {
        id: topbar; x: 60; y: 8; width: parent.width - 72; height: 40
        QuietButton { id: workspaceButton; text: window.overview ? "Overview" : window.spaces[window.workspace]; hint: "Choose workspace"; onClicked: drawer.open() }
        Row {
            anchors.centerIn: parent; spacing: 6; visible: !window.overview && window.workspace < 2
            Repeater {
                model: 3
                QuietButton {
                    required property int index
                    implicitWidth: 32; text: ""; hint: "Terminal " + (index + 1)
                    readonly property int paneIndex: window.workspace * 3 + index
                    Rectangle {
                        anchors.centerIn: parent; width: window.selected === parent.paneIndex ? 24 : 12; height: 4; radius: 2
                        color: window.selected === parent.paneIndex ? window.accent : window.left(parent.paneIndex) + window.paneWidth(parent.paneIndex) > deck.contentX && window.left(parent.paneIndex) < deck.contentX + deck.width ? "#697588" : "#343e4e"
                    }
                    onClicked: window.enterPane(paneIndex)
                }
            }
        }
        Row {
            anchors.right: parent.right; spacing: 6
            QuietButton { text: window.overview ? "Done" : "Overview"; onClicked: window.toggleOverview() }
            QuietButton { text: "☰"; hint: "Application menu"; onClicked: appMenu.open() }
        }
    }
    Flickable {
        id: deck; objectName: "deck"
        x: 60; y: 60; width: parent.width - 72; height: parent.height - 72
        clip: true; boundsBehavior: Flickable.StopAtBounds
        interactive: window.overview
        contentWidth: window.overview ? width : window.stripWidth()
        contentHeight: window.overview ? groupHeight * 2 + 84 : height
        readonly property real previewHeight: Math.min(240, Math.max(150, (height - 100) / 2))
        readonly property real groupHeight: previewHeight + 56
        Behavior on contentX { enabled: !window.overview; NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
        Repeater {
            model: 3
            Label { required property int index; visible: window.overview; x: 0; y: index * deck.groupHeight; text: window.spaces[index] + (index < 2 ? "  ·  3 terminals" : ""); color: "#a8b2c2"; font.pixelSize: 13 }
        }
        Repeater {
            id: panels; model: 6
            delegate: Rectangle {
                id: pane; required property int index
                property alias terminal: terminal
                readonly property int spaceIndex: Math.floor(index / 3)
                readonly property real previewScale: Math.min((deck.width - 24) / 3 / width, deck.previewHeight / height)
                objectName: "pane" + index
                width: window.paneWidth(index); height: deck.height
                x: window.overview ? (index % 3) * (deck.width + 12) / 3 : window.left(index)
                y: window.overview ? spaceIndex * deck.groupHeight + 30 : 0
                scale: window.overview ? previewScale : 1
                transformOrigin: Item.TopLeft
                visible: window.overview || spaceIndex === window.workspace
                color: "#181c23"; radius: 6
                border.width: 1; border.color: window.selected === index ? window.accent : "#323a47"
                Behavior on x { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Text { x: 14; y: 10; width: parent.width - 65; elide: Text.ElideMiddle; text: "~  /  terminal " + (index % 3 + 1); font.pixelSize: 12; color: window.selected === index ? "#d4dfef" : "#8995a7" }
                QuietButton { anchors.right: parent.right; anchors.rightMargin: 3; y: 0; text: "⋯"; hint: "Terminal layout"; enabled: !window.overview; onClicked: { window.focusPane(index); const point = mapToItem(window.contentItem, 0, height); paneMenu.x = Math.max(0, Math.min(point.x, window.width - paneMenu.width - 12)); paneMenu.y = point.y; paneMenu.open() } }
                TerminalItem {
                    id: terminal; objectName: "terminal" + index
                    x: 4; y: 36; width: parent.width - 8; height: parent.height - 40
                    enabled: !window.overview
                    presentationEnabled: pane.visible && pane.x + pane.width * pane.scale > deck.contentX && pane.x < deck.contentX + deck.width && pane.y + pane.height * pane.scale > deck.contentY && pane.y < deck.contentY + deck.height
                    opacity: presentationEnabled ? 1 : 0
                    WheelHandler {
                        orientation: Qt.Horizontal
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        target: null
                        onWheel: event => {
                            const dx = event.pixelDelta.x || event.angleDelta.x
                            if (!window.overview && dx !== 0) {
                                deck.contentX = Math.max(0, Math.min(deck.contentWidth - deck.width, deck.contentX - dx)); event.accepted = true
                            } else event.accepted = false
                        }
                    }
                    Accessible.role: Accessible.Terminal; Accessible.name: "Terminal " + (index + 1) + " in " + window.spaces[pane.spaceIndex]
                    onActiveFocusChanged: if (activeFocus && !window.overview) window.focusPane(index)
                }
                Text { anchors.centerIn: terminal; visible: !terminal.status.startsWith("Attached"); text: terminal.status; color: "#d9be91"; font.pixelSize: 13 }
                MouseArea { anchors.fill: parent; enabled: window.overview; cursorShape: Qt.PointingHandCursor; onClicked: window.enterPane(index) }
            }
        }
        Label { anchors.centerIn: parent; visible: !window.overview && window.workspace === 2; text: "An empty workspace"; color: "#7d899c"; font.pixelSize: 14 }
    }
    Drawer {
        id: drawer; y: 56; width: 248; height: window.height - 56; edge: Qt.LeftEdge
        background: Rectangle { color: "#1b2029"; radius: 8 }
        Column {
            anchors.fill: parent; anchors.margins: 14; spacing: 8
            Label { text: "Workspaces"; color: "#8694aa"; font.pixelSize: 12; padding: 10 }
            Repeater { model: 3; QuietButton { required property int index; width: 220; text: window.spaces[index]; onClicked: window.chooseWorkspace(index) } }
            Label { width: 220; padding: 10; text: "UI study · six disposable shells\nClosing stops these shells."; wrapMode: Text.Wrap; color: "#8694aa"; font.pixelSize: 12 }
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Menu {
        onClosed: if (!window.overview && window.selected >= 0 && !help.visible) window.panel(window.selected).terminal.forceActiveFocus()
        id: appMenu; x: window.width - width - 12; y: 50
        MenuItem { text: "Keyboard shortcuts"; onTriggered: help.open() }
        MenuItem { text: "Reduced motion"; checkable: true; checked: window.reducedMotion; onTriggered: window.reducedMotion = checked }
        MenuSeparator {}
        MenuItem { text: "Copy selection"; enabled: window.selected >= 0; onTriggered: window.panel(window.selected).terminal.copy() }
        MenuItem { text: "Paste"; enabled: window.selected >= 0 && !window.overview; onTriggered: { window.panel(window.selected).terminal.paste(); window.panel(window.selected).terminal.forceActiveFocus() } }
        MenuSeparator {}
        MenuItem { text: "Close UI study"; onTriggered: window.close() }
    }
    Menu {
        id: paneMenu
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
        MenuItem { text: "Cycle width"; onTriggered: window.cycleWidth() }
        MenuItem { text: "Toggle full width"; onTriggered: window.toggleFull() }
    }
    Dialog {
        id: help; title: "Keyboard shortcuts"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
        contentItem: Label { padding: 18; text: window.chordLabel + "← / →     Previous / next terminal\n" + window.chordLabel + "↑ / ↓     Previous / next workspace\n" + window.chordLabel + "O             Overview\n" + window.chordLabel + "R             Cycle ½ → ⅔ → ⅓\n" + window.chordLabel + "F              Full width / restore\n" + window.chordLabel + "/              This help\nEscape             Close Overview or overlay"; lineHeight: 1.6 }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Shortcut { sequence: window.chord + "Left"; onActivated: window.navigate(-1) }
    Shortcut { sequence: window.chord + "Right"; onActivated: window.navigate(1) }
    Shortcut { sequence: window.chord + "Up"; onActivated: window.chooseWorkspace(window.workspace - 1) }
    Shortcut { sequence: window.chord + "Down"; onActivated: window.chooseWorkspace(window.workspace + 1) }
    Shortcut { sequence: window.chord + "O"; onActivated: window.toggleOverview() }
    Shortcut { sequence: window.chord + "R"; enabled: !window.overview; onActivated: window.cycleWidth() }
    Shortcut { sequence: window.chord + "F"; enabled: !window.overview; onActivated: window.toggleFull() }
    Shortcut { sequence: window.chord + "/"; onActivated: help.open() }
    Shortcut { sequence: "Escape"; enabled: window.overview && !help.visible && !drawer.visible && !appMenu.visible; onActivated: window.toggleOverview() }
    Component.onCompleted: Qt.callLater(function() { window.panel(0).terminal.forceActiveFocus() })
}
