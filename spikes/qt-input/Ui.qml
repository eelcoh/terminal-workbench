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
    property bool pendingWorkspaceDialog: false
    property var widths: []
    property var fullWidths: []
    property var remembered: []
    property var positions: []
    property var spaces: []
    property var spaceIds: []
    property var directories: []
    property var counts: []
    property var terminalItems: []
    readonly property bool creationOpen: createDialog.visible
    readonly property bool drawerOpen: drawer.visible
    readonly property bool nameFocused: newName.activeFocus
    readonly property bool canCreateTerminal: spaces.length > 0 && counts[workspace] < 8 && !studySession.busy
    readonly property bool canCreateWorkspace: spaces.length < 8 && !studySession.busy
    ListModel { id: terminalModel }
    function indices(space) { let result = []; for (let i = 0; i < terminalModel.count; ++i) if (terminalModel.get(i).spaceIndex === space) result.push(i); return result }
    function ordinal(i) { return terminalModel.get(i).ordinal }
    function applyCatalog(catalog, method) {
        let names = [], ids = [], folders = [], totals = []
        let sizes = widths.slice(), full = fullWidths.slice(), focus = remembered.slice(), scroll = positions.slice()
        let added = -1
        for (let w = 0; w < catalog.workspaces.length; ++w) {
            const space = catalog.workspaces[w]
            names.push(space.name); ids.push(space.id); folders.push(space.directory || studySession.defaultDirectory); totals.push(space.terminals.length)
            if (focus[w] === undefined) { focus.push(-1); scroll.push(0) }
            for (let j = 0; j < space.terminals.length; ++j) {
                const terminal = space.terminals[j]
                let existing = -1
                for (let k = 0; k < terminalModel.count; ++k) if (terminalModel.get(k).terminalId === terminal.id) { existing = k; break }
                if (existing >= 0) continue
                sizes.push(0.5); full.push(false); widths = sizes; fullWidths = full
                terminalModel.append({terminalId: terminal.id, spaceIndex: w, ordinal: j})
                added = terminalModel.count - 1
                if (focus[w] < 0) focus[w] = added
            }
        }
        spaces = names; spaceIds = ids; directories = folders; counts = totals
        remembered = focus; positions = scroll
        if (method === "workspace.create_at") { createDialog.close(); chooseWorkspace(spaces.length - 1); if (overview) toggleOverview() }
        if (method === "terminal.create" && added >= 0) { if (overview) toggleOverview(); Qt.callLater(function() { focusPane(added) }) }
    }
    function newTerminal() { if (canCreateTerminal && !createDialog.visible) studySession.createTerminal(spaceIds[workspace]) }
    function newWorkspace() {
        if (!canCreateWorkspace || createDialog.visible) return
        pendingWorkspaceDialog = true
        if (drawer.visible) drawer.close()
        else if (appMenu.visible) appMenu.close()
        else { pendingWorkspaceDialog = false; openWorkspaceForm() }
    }
    function openWorkspaceForm() {
        newName.text = ""; newFolder.text = directories[workspace] || studySession.defaultDirectory
        createDialog.open()
    }
    function restoreAfterPopup() {
        if (pendingWorkspaceDialog) {
            pendingWorkspaceDialog = false; Qt.callLater(openWorkspaceForm)
        } else if (!overview && selected >= 0 && !createDialog.visible && !help.visible) panel(selected).terminal.forceActiveFocus()
    }
    Connections {
        target: studySession
        function onCompleted(catalog, method) { window.applyCatalog(catalog, method) }
    }
    readonly property string chord: Qt.platform.os === "osx" ? "Ctrl+Alt+" : "Meta+Alt+"
    readonly property string chordLabel: Qt.platform.os === "osx" ? "⌘⌥" : "Super+Alt+"
    readonly property color accent: "#b4c9ec"
    readonly property int motion: reducedMotion ? 0 : 220
    function panel(i) { return panels.itemAt(i) }
    function paneWidth(i) { return Math.max(240, (deck.width + 12) * (fullWidths[i] ? 1 : widths[i]) - 12) }
    function left(i) { let x = 0; const items = indices(terminalModel.get(i).spaceIndex); for (let j of items) { if (j === i) break; x += paneWidth(j) + 12 } return x }
    function stripWidth() { const items = indices(workspace); return items.length === 0 ? deck.width : Math.max(deck.width, left(items[items.length - 1]) + paneWidth(items[items.length - 1])) }
    function reveal() {
        if (overview || selected < 0) return
        let x = left(selected), w = paneWidth(selected)
        if (x < deck.contentX) deck.contentX = x
        else if (x + w > deck.contentX + deck.width) deck.contentX = Math.max(0, x + w - deck.width)
    }
    function focusPane(i) {
        if (i < 0 || i >= terminalModel.count) return
        selected = i; workspace = terminalModel.get(i).spaceIndex
        let values = remembered.slice(); values[workspace] = i; remembered = values
        if (!overview) { reveal(); panel(i).terminal.forceActiveFocus() }
    }
    function chooseWorkspace(i) {
        let values = positions.slice(); values[workspace] = deck.contentX; positions = values
        workspace = Math.max(0, Math.min(spaces.length - 1, i)); selected = remembered[workspace] === undefined ? -1 : remembered[workspace]
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
        const items = indices(workspace); if (items.length) focusPane(items[Math.max(0, Math.min(items.length - 1, items.indexOf(selected) + delta))])
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
        QuietButton { id: workspaceButton; text: window.overview ? "Overview" : (window.spaces[window.workspace] || ""); hint: "Choose workspace"; onClicked: drawer.open() }
        Row {
            anchors.centerIn: parent; spacing: 6; visible: !window.overview && window.counts[window.workspace] > 0
            Repeater {
                model: window.counts[window.workspace] || 0
                QuietButton {
                    required property int index
                    implicitWidth: 32; text: ""; hint: "Terminal " + (index + 1)
                    readonly property int paneIndex: window.indices(window.workspace)[index]
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
            QuietButton { text: studySession.busy ? "…" : "+"; hint: "New terminal · " + window.chordLabel + "T"; enabled: window.canCreateTerminal; onClicked: window.newTerminal() }
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
        contentHeight: window.overview ? overviewHeight() : height
        readonly property real previewHeight: Math.min(240, Math.max(150, (height - 100) / 2))
        readonly property real groupHeight: previewHeight + 56
        function rows(space) { return Math.max(1, Math.ceil((window.counts[space] || 0) / 3)) }
        function groupTop(space) { let y = 0; for (let i = 0; i < space; ++i) y += rows(i) * (previewHeight + 12) + 44; return y }
        function overviewHeight() { return Math.max(height, groupTop(window.spaces.length)) }
        Behavior on contentX { enabled: !window.overview; NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
        Repeater {
            model: window.spaces.length
            Label { required property int index; visible: window.overview; x: 0; y: deck.groupTop(index); text: window.spaces[index] + (window.counts[index] > 0 ? "  ·  " + window.counts[index] + " terminals" : ""); color: "#a8b2c2"; font.pixelSize: 13 }
        }
        Repeater {
            id: panels; model: terminalModel
            delegate: Rectangle {
                id: pane; required property int index
                required property int spaceIndex
                required property int ordinal
                required property string terminalId
                property alias terminal: terminal
                readonly property real previewScale: Math.min((deck.width - 24) / 3 / width, deck.previewHeight / height)
                objectName: "pane" + index
                width: window.paneWidth(index); height: deck.height
                x: window.overview ? (ordinal % 3) * (deck.width + 12) / 3 : window.left(index)
                y: window.overview ? deck.groupTop(spaceIndex) + Math.floor(ordinal / 3) * (deck.previewHeight + 12) + 30 : 0
                scale: window.overview ? previewScale : 1
                transformOrigin: Item.TopLeft
                visible: window.overview || spaceIndex === window.workspace
                color: "#181c23"; radius: 6
                border.width: 1; border.color: window.selected === index ? window.accent : "#323a47"
                Behavior on x { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Text { x: 14; y: 10; width: parent.width - 65; elide: Text.ElideMiddle; text: "~  /  terminal " + (ordinal + 1); font.pixelSize: 12; color: window.selected === index ? "#d4dfef" : "#8995a7" }
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
                    Component.onCompleted: {
                        studySession.attach(terminal, pane.terminalId)
                        let items = window.terminalItems.slice(); items.push(terminal); window.terminalItems = items
                    }
                    onActiveFocusChanged: if (activeFocus && !window.overview) window.focusPane(index)
                }
                Text { anchors.centerIn: terminal; visible: !terminal.status.startsWith("Attached"); text: terminal.status; color: "#d9be91"; font.pixelSize: 13 }
                MouseArea { anchors.fill: parent; enabled: window.overview; cursorShape: Qt.PointingHandCursor; onClicked: window.enterPane(index) }
            }
        }
        Label { anchors.centerIn: parent; visible: !window.overview && (window.counts[window.workspace] || 0) === 0; text: "An empty workspace"; color: "#7d899c"; font.pixelSize: 14 }
    }
    Drawer {
        id: drawer; y: 56; width: 248; height: window.height - 56; edge: Qt.LeftEdge
        background: Rectangle { color: "#1b2029"; radius: 8 }
        Column {
            anchors.fill: parent; anchors.margins: 14; spacing: 8
            Label { text: "Workspaces"; color: "#8694aa"; font.pixelSize: 12; padding: 10 }
            Repeater { model: window.spaces.length; QuietButton { required property int index; width: 220; text: window.spaces[index]; onClicked: window.chooseWorkspace(index) } }
            QuietButton { objectName: "newWorkspaceButton"; width: 220; text: "New workspace…"; enabled: window.canCreateWorkspace; onClicked: window.newWorkspace() }
            Label { width: 220; padding: 10; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: "#8694aa"; font.pixelSize: 12 }
        }
        onClosed: window.restoreAfterPopup()
    }
    Menu {
        onClosed: window.restoreAfterPopup()
        id: appMenu; x: window.width - width - 12; y: 50
        MenuItem { text: "New terminal"; enabled: window.canCreateTerminal; onTriggered: window.newTerminal() }
        MenuItem { text: "New workspace…"; enabled: window.canCreateWorkspace; onTriggered: window.newWorkspace() }
        MenuSeparator {}
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
    Rectangle {
        visible: studySession.error.length > 0 && !createDialog.visible
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(window.width - 100, 680); height: 76; color: "#352b22"; radius: 8; z: 4
        Label { x: 16; y: 12; width: parent.width - 120; text: studySession.error; wrapMode: Text.Wrap; color: "#f0d6b8" }
        QuietButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Refresh"; enabled: !studySession.busy; onClicked: studySession.refresh() }
    }
    component ProjectField: TextField {
        selectByMouse: true; color: "#e0e6ef"; selectionColor: window.accent; selectedTextColor: "#101318"
        placeholderTextColor: "#7d899c"; padding: 10
        background: Rectangle { radius: 6; color: "#101318"; border.color: parent.activeFocus ? window.accent : "#3a4657" }
    }
    Dialog {
        id: createDialog; objectName: "createWorkspaceDialog"
        onOpened: newName.forceActiveFocus()
        title: "New workspace"; anchors.centerIn: parent; modal: true; width: Math.min(460, window.width - 80)
        padding: 20
        background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
        header: Label { text: createDialog.title; padding: 20; bottomPadding: 6; color: "#e0e6ef"; font.pixelSize: 18 }
        closePolicy: studySession.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
        contentItem: Column {
            spacing: 12
            Label { text: "Name"; color: "#c2c9d4" }
            ProjectField { id: newName; objectName: "workspaceNameInput"; width: parent.width; placeholderText: "Project name"; enabled: !studySession.busy; selectByMouse: true; maximumLength: 63; onAccepted: newFolder.forceActiveFocus() }
            Label { text: "Folder"; color: "#c2c9d4" }
            ProjectField { id: newFolder; objectName: "workspaceFolderInput"; width: parent.width; enabled: !studySession.busy; selectByMouse: true; onAccepted: if (submitWorkspace.enabled) submitWorkspace.clicked() }
            Label { width: parent.width; text: studySession.error; visible: text.length > 0; wrapMode: Text.Wrap; color: "#e3bf91" }
            Row {
                spacing: 8
                QuietButton { text: "Cancel"; enabled: !studySession.busy; onClicked: createDialog.close() }
                QuietButton { id: submitWorkspace; text: studySession.busy ? "Creating…" : "Create"; enabled: !studySession.busy && newName.text.trim().length > 0 && newFolder.text.trim().length > 0; onClicked: studySession.createWorkspace(newName.text, newFolder.text) }
            }
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Dialog {
        id: help; title: "Keyboard shortcuts"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
        contentItem: Label { padding: 18; text: window.chordLabel + "← / →     Previous / next terminal\n" + window.chordLabel + "↑ / ↓     Previous / next workspace\n" + window.chordLabel + "T              New terminal\n" + window.chordLabel + "N             New workspace\n" + window.chordLabel + "O             Overview\n" + window.chordLabel + "R             Cycle ½ → ⅔ → ⅓\n" + window.chordLabel + "F              Full width / restore\n" + window.chordLabel + "/              This help\nEscape             Close Overview or overlay"; lineHeight: 1.6 }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Shortcut { sequence: window.chord + "T"; enabled: !createDialog.visible; onActivated: window.newTerminal() }
    Shortcut { sequence: window.chord + "N"; enabled: !createDialog.visible; onActivated: window.newWorkspace() }
    Shortcut { sequence: window.chord + "Left"; enabled: !createDialog.visible; onActivated: window.navigate(-1) }
    Shortcut { sequence: window.chord + "Right"; enabled: !createDialog.visible; onActivated: window.navigate(1) }
    Shortcut { sequence: window.chord + "Up"; enabled: !createDialog.visible; onActivated: window.chooseWorkspace(window.workspace - 1) }
    Shortcut { sequence: window.chord + "Down"; enabled: !createDialog.visible; onActivated: window.chooseWorkspace(window.workspace + 1) }
    Shortcut { sequence: window.chord + "O"; enabled: !createDialog.visible; onActivated: window.toggleOverview() }
    Shortcut { sequence: window.chord + "R"; enabled: !window.overview && !createDialog.visible; onActivated: window.cycleWidth() }
    Shortcut { sequence: window.chord + "F"; enabled: !window.overview && !createDialog.visible; onActivated: window.toggleFull() }
    Shortcut { sequence: window.chord + "/"; enabled: !createDialog.visible; onActivated: help.open() }
    Shortcut { sequence: "Escape"; enabled: window.overview && !createDialog.visible && !help.visible && !drawer.visible && !appMenu.visible; onActivated: window.toggleOverview() }
    Component.onCompleted: { window.applyCatalog(studySession.initialCatalog, "initialize"); selected = remembered[0]; Qt.callLater(function() { if (selected >= 0) window.panel(selected).terminal.forceActiveFocus() }) }
}
