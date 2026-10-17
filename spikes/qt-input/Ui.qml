import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as NativeDialogs
import Workbench.Probe
ApplicationWindow {
    id: window
    width: 1240; height: 760; minimumWidth: 680; minimumHeight: 440
    visible: true; title: "Terminal Workbench · Qt UI study"; color: chrome.background
    property int workspace: 0
    property int selected: 0
    property bool overview: false
    property bool reducedMotion: false
    property bool pendingWorkspaceDialog: false
    property string workspaceFormMode: "create"
    property string editingWorkspace: ""
    readonly property bool folderFocused: newFolder.activeFocus
    property var folderBrowser: null
    readonly property bool folderBrowserOpen: folderBrowser !== null && folderBrowser.visible
    readonly property url folderBrowserLocation: folderBrowser ? folderBrowser.currentFolder : ""
    readonly property url folderBrowserSelection: folderBrowser ? folderBrowser.selectedFolder : ""
    property bool pendingAppearance: false
    property var workspaceThemes: []
    property string appearanceWorkspace: ""
    property int previewTheme: 0
    readonly property bool appearanceOpen: appearance.visible
    readonly property bool applicationMenuOpen: appMenu.visible
    readonly property bool themeSearchFocus: appearance.searchFocused
    readonly property int themeMatchCount: appearance.matches.length
    readonly property var chrome: studySession.themes[themeIndexFor(workspace)]
    function themeIndexFor(space) { return appearance.visible && spaceIds[space] === appearanceWorkspace ? previewTheme : (workspaceThemes[space] || 0) }
    function openAppearance() {
        if (overview || createDialog.visible || appearance.visible) return
        if (appMenu.visible) { pendingAppearance = true; appMenu.close() }
        else showAppearance()
    }
    function showAppearance() {
        appearanceWorkspace = spaceIds[workspace]; previewTheme = workspaceThemes[workspace] || 0; appearance.open()
    }
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
    readonly property bool canCreateTerminal: spaces.length > 0 && counts[workspace] < 8 && !studySession.busy && dragSlot < 0
    readonly property bool canCreateWorkspace: spaces.length < 8 && !studySession.busy && dragSlot < 0
    ListModel { id: terminalModel }
    property int catalogRevision: 0
    function indices(space) { const revision = catalogRevision; let result = []; for (let i = 0; i < terminalModel.count; ++i) if (terminalModel.get(i).spaceIndex === space) result.push(i); return result.sort((a,b) => terminalModel.get(a).ordinal - terminalModel.get(b).ordinal) }
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
                if (existing >= 0) {
                    terminalModel.setProperty(existing, "spaceIndex", w)
                    terminalModel.setProperty(existing, "ordinal", j)
                    continue
                }
                sizes.push(0.5); full.push(false); widths = sizes; fullWidths = full
                terminalModel.append({terminalId: terminal.id, spaceIndex: w, ordinal: j})
                added = terminalModel.count - 1
                if (focus[w] < 0) focus[w] = added
            }
        }
        spaces = names; spaceIds = ids; directories = folders; counts = totals
        ++catalogRevision
        for (let w = 0; w < names.length; ++w) {
            const items = indices(w)
            if (items.indexOf(focus[w]) < 0) focus[w] = items.length ? items[0] : -1
        }
        remembered = focus; positions = scroll
        if (selected >= 0) {
            workspace = terminalModel.get(selected).spaceIndex
            let values = remembered.slice(); values[workspace] = selected; remembered = values
            if (!overview) Qt.callLater(reveal)
        }
        if (method === "workspace.create_at") { createDialog.close(); chooseWorkspace(spaces.length - 1); if (overview) toggleOverview() }
        if (method === "workspace.rename" || method === "workspace.directory") createDialog.close()
        if (method === "terminal.create" && added >= 0) { if (overview) toggleOverview(); Qt.callLater(function() { focusPane(added) }) }
    }
    function newTerminal() { if (canCreateTerminal && !createDialog.visible && !window.appearanceOpen) studySession.createTerminal(spaceIds[workspace]) }
    function newWorkspace() {
        if (!canCreateWorkspace || createDialog.visible) return
        workspaceFormMode = "create"; editingWorkspace = ""
        queueWorkspaceForm()
    }
    function editWorkspace(mode) {
        if (studySession.busy || dragSlot >= 0 || createDialog.visible || overview || appearance.visible) return
        workspaceFormMode = mode; editingWorkspace = spaceIds[workspace]
        queueWorkspaceForm()
    }
    function queueWorkspaceForm() {
        pendingWorkspaceDialog = true
        if (drawer.visible) drawer.close()
        else if (appMenu.visible) appMenu.close()
        else { pendingWorkspaceDialog = false; openWorkspaceForm() }
    }
    function openWorkspaceForm() {
        const target = workspaceFormMode === "create" ? workspace : spaceIds.indexOf(editingWorkspace)
        if (target < 0) return
        studySession.clearError()
        newName.text = workspaceFormMode === "create" ? "" : spaces[target]
        newFolder.text = directories[target] || studySession.defaultDirectory
        createDialog.open()
    }
    function browseWorkspaceFolder() {
        if (studySession.busy || !createDialog.visible || workspaceFormMode === "rename") return
        // Qt retains navigation after a dialog's first show. Honor the field on every opening.
        if (folderBrowser) folderBrowser.destroy()
        folderBrowser = folderBrowserFactory.createObject(createDialog, {currentFolder: studySession.folderStart(newFolder.text)})
        if (folderBrowser) folderBrowser.open()
    }
    function restoreFolderFocus() { if (createDialog.visible) newFolder.forceActiveFocus() }
    function submitWorkspaceForm() {
        if (workspaceFormMode === "rename") studySession.renameWorkspace(editingWorkspace, newName.text)
        else if (workspaceFormMode === "folder") studySession.setWorkspaceDirectory(editingWorkspace, newFolder.text)
        else studySession.createWorkspace(newName.text, newFolder.text)
    }
    function restoreAfterPopup() {
        if (pendingAppearance) {
            pendingAppearance = false; Qt.callLater(showAppearance)
        } else if (pendingWorkspaceDialog) {
            pendingWorkspaceDialog = false; Qt.callLater(openWorkspaceForm)
        } else if (!overview && selected >= 0 && !createDialog.visible && !window.appearanceOpen && !appearance.visible && !help.visible) panel(selected).terminal.forceActiveFocus()
    }
    Connections {
        target: studySession
        function onCompleted(catalog, method) { window.applyCatalog(catalog, method) }
    }
    readonly property string chord: Qt.platform.os === "osx" ? "Ctrl+Alt+" : "Meta+Alt+"
    readonly property string chordLabel: Qt.platform.os === "osx" ? "⌘⌥" : "Super+Alt+"
    readonly property color accent: chrome.accent
    readonly property int motion: reducedMotion ? 0 : 220
    function panel(i) { return panels.itemAt(i) }
    function paneWidth(i) { return Math.max(240, (deck.width + 12) * (fullWidths[i] ? 1 : widths[i]) - 12) }
    function left(i) { if (i < 0) return 0; let x = 0; const items = indices(terminalModel.get(i).spaceIndex); for (let j of items) { if (j === i) break; x += paneWidth(j) + 12 } return x }
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
    // Drag is client presentation only. Commit one existing service mutation on drop.
    property int dragSlot: -1
    property int dropSpace: -1
    property int dropPosition: -1
    property int dropRaw: -1
    property point dragPoint: Qt.point(0, 0)
    property bool dropAllowed: false
    function cancelDrag() { dragSlot = -1; dropSpace = -1; dropPosition = -1; dropAllowed = false }
    function updateDrop(point) {
        dragPoint = point; dropSpace = -1; dropPosition = -1; dropAllowed = false
        if (point.x < 0 || point.x > deck.width || point.y < 0 || point.y > deck.height) return
        const y = point.y + deck.contentY
        for (let w = 0; w < spaces.length; ++w) {
            if (y < deck.groupTop(w) || y >= deck.groupTop(w + 1)) continue
            dropSpace = w
            const row = Math.max(0, Math.floor((y - deck.groupTop(w) - 30) / (deck.previewHeight + 12)))
            const cellWidth = (deck.width + 12) / 3
            const col = Math.max(0, Math.min(2, Math.floor(point.x / cellWidth)))
            const items = indices(w), slot = items[row * 3 + col]
            const half = slot === undefined ? cellWidth / 2 : panel(slot).width * panel(slot).previewScale / 2
            const raw = Math.min(counts[w], row * 3 + col + (point.x - col * cellWidth >= half ? 1 : 0))
            const source = terminalModel.get(dragSlot)
            dropRaw = raw
            dropPosition = raw - (source.spaceIndex === w && source.ordinal < raw ? 1 : 0)
            dropAllowed = source.spaceIndex === w || counts[w] < 8
            break
        }
    }
    function finishDrag() {
        if (dragSlot >= 0 && dropAllowed) {
            const source = terminalModel.get(dragSlot)
            if (source.spaceIndex !== dropSpace || source.ordinal !== dropPosition)
                studySession.moveTerminal(source.terminalId, spaceIds[dropSpace], dropPosition)
        }
        cancelDrag()
    }
    function toggleOverview() {
        cancelDrag()
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
        property bool fixedPalette: false
        implicitWidth: Math.max(36, contentItem.implicitWidth + 22); implicitHeight: 36
        hoverEnabled: true; focusPolicy: Qt.StrongFocus
        Accessible.name: hint
        contentItem: Text { text: control.text; color: control.fixedPalette ? "#c2c9d4" : window.chrome.foreground; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 7; color: control.down || control.hovered || control.visualFocus ? (control.fixedPalette ? "#323b4b" : window.chrome.hover) : "transparent"; border.width: control.visualFocus ? 1 : 0; border.color: window.accent }
        ToolTip.visible: hovered; ToolTip.delay: 650; ToolTip.text: hint
    }
    Rectangle { x: 48; y: 0; width: 1; height: parent.height; color: window.chrome.border }
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
                    readonly property int paneIndex: window.indices(window.workspace)[index] ?? -1
                    Rectangle {
                        anchors.centerIn: parent; width: window.selected === parent.paneIndex ? 24 : 12; height: 4; radius: 2
                        color: window.selected === parent.paneIndex ? window.accent : window.left(parent.paneIndex) + window.paneWidth(parent.paneIndex) > deck.contentX && window.left(parent.paneIndex) < deck.contentX + deck.width ? window.chrome.muted : window.chrome.border
                    }
                    onClicked: window.enterPane(paneIndex)
                }
            }
        }
        Row {
            anchors.right: parent.right; spacing: 6
            QuietButton { text: studySession.busy ? "…" : "+"; hint: "New terminal · " + window.chordLabel + "T"; enabled: window.canCreateTerminal; onClicked: window.newTerminal() }
            QuietButton { text: window.overview ? "Done" : "Overview"; onClicked: window.toggleOverview() }
            QuietButton { objectName: "applicationMenuButton"; text: "☰"; hint: "Application menu"; onClicked: appMenu.open() }
        }
    }
    Flickable {
        id: deck; objectName: "deck"
        x: 60; y: 60; width: parent.width - 72; height: parent.height - 72
        clip: true; boundsBehavior: Flickable.StopAtBounds
        interactive: window.overview && window.dragSlot < 0
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
            Label { required property int index; visible: window.overview; x: 0; y: deck.groupTop(index); text: window.spaces[index] + (window.counts[index] > 0 ? "  ·  " + window.counts[index] + " terminals" : ""); color: window.chrome.muted; font.pixelSize: 13 }
        }
        Repeater {
            id: panels; model: terminalModel
            delegate: Rectangle {
                id: pane; required property int index
                required property int spaceIndex
                required property int ordinal
                required property string terminalId
                property alias terminal: terminal
                opacity: window.dragSlot === index ? 0.45 : 1
                readonly property real previewScale: Math.min((deck.width - 24) / 3 / width, deck.previewHeight / height)
                objectName: "pane" + index
                width: window.paneWidth(index); height: deck.height
                x: window.overview ? (ordinal % 3) * (deck.width + 12) / 3 : window.left(index)
                y: window.overview ? deck.groupTop(spaceIndex) + Math.floor(ordinal / 3) * (deck.previewHeight + 12) + 30 : 0
                scale: window.overview ? previewScale : 1
                transformOrigin: Item.TopLeft
                visible: window.overview || spaceIndex === window.workspace
                color: studySession.themes[window.themeIndexFor(spaceIndex)].surface; radius: 6
                border.width: 1; border.color: window.selected === index ? studySession.themes[window.themeIndexFor(spaceIndex)].accent : studySession.themes[window.themeIndexFor(spaceIndex)].border
                Behavior on x { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: window.motion; easing.type: Easing.OutCubic } }
                Text { x: 14; y: 10; width: parent.width - 65; elide: Text.ElideMiddle; text: "~  /  terminal " + (ordinal + 1); font.pixelSize: 12; color: studySession.themes[window.themeIndexFor(spaceIndex)].foreground }
                QuietButton { anchors.right: parent.right; anchors.rightMargin: 3; y: 0; text: "⋯"; hint: "Terminal layout"; enabled: !window.overview; onClicked: { window.focusPane(index); const point = mapToItem(window.contentItem, 0, height); paneMenu.x = Math.max(0, Math.min(point.x, window.width - paneMenu.width - 12)); paneMenu.y = point.y; paneMenu.open() } }
                TerminalItem {
                    id: terminal; objectName: "terminal" + index
                    property int workspaceIndex: pane.spaceIndex
                    property int orderInWorkspace: pane.ordinal
                    property string stableIdentity: pane.terminalId
                    x: 4; y: 36; width: parent.width - 8; height: parent.height - 40
                    enabled: !window.overview
                    themeIndex: window.themeIndexFor(pane.spaceIndex)
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
                MouseArea {
                    anchors.fill: parent; enabled: window.overview && !studySession.busy
                    cursorShape: window.dragSlot >= 0 ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    preventStealing: true
                    property point pressedAt
                    property bool moved: false
                    onPressed: mouse => { pressedAt = mapToItem(deck, mouse.x, mouse.y); moved = false }
                    onPositionChanged: mouse => {
                        if (!pressed) return
                        const point = mapToItem(deck, mouse.x, mouse.y)
                        if (!moved && Math.abs(point.x - pressedAt.x) + Math.abs(point.y - pressedAt.y) >= Qt.styleHints.startDragDistance) {
                            moved = true; window.dragSlot = index
                        }
                        if (moved && window.dragSlot === index) window.updateDrop(point)
                    }
                    onReleased: { if (moved) window.finishDrag(); else window.enterPane(index) }
                    onCanceled: window.cancelDrag()
                }
            }
        }
        Label { anchors.centerIn: parent; visible: !window.overview && (window.counts[window.workspace] || 0) === 0; text: "An empty workspace"; color: window.chrome.muted; font.pixelSize: 14 }
    }
    Item {
        x: deck.x; y: deck.y; width: deck.width; height: deck.height; clip: true
        Rectangle {
            visible: window.dragSlot >= 0 && window.dropSpace >= 0
            x: 0; y: deck.groupTop(window.dropSpace) - deck.contentY
            width: deck.width; height: deck.groupTop(window.dropSpace + 1) - deck.groupTop(window.dropSpace) - 8
            color: "transparent"; radius: 8; border.width: 1
            border.color: window.dropAllowed ? window.accent : "#e49b9b"
        }
        Rectangle {
            visible: window.dragSlot >= 0 && window.dropSpace >= 0 && window.dropAllowed
            readonly property int total: window.counts[window.dropSpace] || 0
            readonly property int before: Math.min(window.dropRaw, Math.max(0, total - 1))
            readonly property int slot: window.dropSpace < 0 ? -1 : (window.indices(window.dropSpace)[before] ?? -1)
            x: slot < 0 ? 6 : (before % 3) * (deck.width + 12) / 3 + (window.dropRaw === total ? window.panel(slot).width * window.panel(slot).previewScale + 5 : -5)
            y: deck.groupTop(window.dropSpace) + Math.floor(before / 3) * (deck.previewHeight + 12) + 30 - deck.contentY
            width: 3; height: deck.previewHeight; radius: 1.5; color: window.accent
        }
    }
    Rectangle {
        visible: window.dragSlot >= 0
        x: Math.min(window.width - width - 12, deck.x + window.dragPoint.x + 16)
        y: Math.max(60, Math.min(window.height - height - 12, deck.y + window.dragPoint.y + 16))
        width: dragLabel.implicitWidth + 24; height: 38; radius: 8; color: window.chrome.surface; border.color: window.accent; z: 5
        Label { id: dragLabel; anchors.centerIn: parent; text: (window.dropSpace < 0 ? "Move terminal" : !window.dropAllowed ? "Workspace is full" : window.spaces[window.dropSpace] + " · position " + (window.dropPosition + 1)) + " · Esc cancels"; color: window.chrome.foreground }
    }
    Timer {
        interval: 16; repeat: true; running: window.dragSlot >= 0
        onTriggered: {
            const y = window.dragPoint.y
            const delta = y < 56 ? -Math.min(12, (56 - y) / 5) : y > deck.height - 56 ? Math.min(12, (y - deck.height + 56) / 5) : 0
            if (delta && window.dragPoint.x >= 0 && window.dragPoint.x <= deck.width) { deck.contentY = Math.max(0, Math.min(deck.contentHeight - deck.height, deck.contentY + delta)); window.updateDrop(window.dragPoint) }
        }
    }
    Drawer {
        id: drawer; y: 56; width: 248; height: window.height - 56; edge: Qt.LeftEdge
        background: Rectangle { color: window.chrome.surface; radius: 8 }
        Column {
            anchors.fill: parent; anchors.margins: 14; spacing: 8
            Label { text: "Workspaces"; color: window.chrome.muted; font.pixelSize: 12; padding: 10 }
            Repeater { model: window.spaces.length; QuietButton { required property int index; width: 220; text: window.spaces[index]; onClicked: window.chooseWorkspace(index) } }
            QuietButton { objectName: "newWorkspaceButton"; width: 220; text: "New workspace…"; enabled: window.canCreateWorkspace; onClicked: window.newWorkspace() }
            Label { width: 220; padding: 10; text: "Disposable UI study\nClosing stops all study shells."; wrapMode: Text.Wrap; color: window.chrome.muted; font.pixelSize: 12 }
        }
        onClosed: window.restoreAfterPopup()
    }
    Menu {
        onClosed: window.restoreAfterPopup()
        id: appMenu; x: window.width - width - 12; y: 50
        MenuItem { text: "New terminal"; enabled: window.canCreateTerminal; onTriggered: window.newTerminal() }
        MenuItem { text: "New workspace…"; enabled: window.canCreateWorkspace; onTriggered: window.newWorkspace() }
        MenuSeparator {}
        MenuItem { objectName: "renameWorkspaceMenuItem"; text: "Rename workspace…"; enabled: !window.overview && !studySession.busy; onTriggered: window.editWorkspace("rename") }
        MenuItem { objectName: "workspaceFolderMenuItem"; text: "Workspace folder…"; enabled: !window.overview && !studySession.busy; onTriggered: window.editWorkspace("folder") }
        MenuSeparator {}
        MenuItem { objectName: "appearanceMenuItem"; text: "Appearance…"; enabled: !window.overview; onTriggered: window.openAppearance() }
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
        visible: studySession.error.length > 0 && !createDialog.visible && !window.appearanceOpen
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(window.width - 100, 680); height: 76; color: "#352b22"; radius: 8; z: 4
        Label { x: 16; y: 12; width: parent.width - 120; text: studySession.error; wrapMode: Text.Wrap; color: "#f0d6b8" }
        QuietButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "Refresh"; enabled: !studySession.busy; onClicked: studySession.refresh() }
    }
    ThemePicker {
        id: appearance; anchors.centerIn: parent
        catalog: studySession.themes; themeIndex: window.previewTheme
        workspaceName: window.spaces[window.spaceIds.indexOf(window.appearanceWorkspace)] || ""
        onPreviewSelected: index => window.previewTheme = index
        onApplySelected: {
            const space = window.spaceIds.indexOf(window.appearanceWorkspace)
            if (space >= 0) { let themes = window.workspaceThemes.slice(); themes[space] = window.previewTheme; window.workspaceThemes = themes }
            close()
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    component ProjectField: TextField {
        selectByMouse: true; color: "#e0e6ef"; selectionColor: window.accent; selectedTextColor: "#101318"
        placeholderTextColor: "#7d899c"; padding: 10
        background: Rectangle { radius: 6; color: "#101318"; border.color: parent.activeFocus ? window.accent : "#3a4657" }
    }
    Dialog {
        id: createDialog; objectName: "createWorkspaceDialog"
        onOpened: {
            const field = window.workspaceFormMode === "folder" ? newFolder : newName
            field.forceActiveFocus(); field.deselect(); field.cursorPosition = field.text.length
        }
        implicitWidth: 460
        title: window.workspaceFormMode === "rename" ? "Rename workspace" : window.workspaceFormMode === "folder" ? "Workspace folder" : "New workspace"; anchors.centerIn: parent; modal: true; width: Math.min(460, window.width - 80)
        padding: 20
        background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
        header: Label { text: createDialog.title; padding: 20; bottomPadding: 6; color: "#e0e6ef"; font.pixelSize: 18 }
        closePolicy: studySession.busy || window.folderBrowserOpen ? Popup.NoAutoClose : Popup.CloseOnEscape
        contentItem: Column {
            spacing: 12
            Label { text: "Name"; visible: window.workspaceFormMode !== "folder"; color: "#c2c9d4" }
            ProjectField {
                id: newName; objectName: "workspaceNameInput"; width: parent.width; placeholderText: "Project name"
                visible: window.workspaceFormMode !== "folder"; enabled: !studySession.busy; maximumLength: 63
                onAccepted: { if (window.workspaceFormMode === "rename") { if (submitWorkspace.enabled) submitWorkspace.clicked() } else newFolder.forceActiveFocus() }
            }
            Label { text: "Folder"; visible: window.workspaceFormMode !== "rename"; color: "#c2c9d4" }
            Row {
                width: parent.width; spacing: 8; visible: window.workspaceFormMode !== "rename"
                ProjectField { id: newFolder; objectName: "workspaceFolderInput"; width: parent.width - browseFolder.width - parent.spacing; enabled: !studySession.busy; onAccepted: if (submitWorkspace.enabled) submitWorkspace.clicked() }
                QuietButton { id: browseFolder; objectName: "browseWorkspaceFolderButton"; fixedPalette: true; text: "Browse…"; hint: "Choose a folder"; enabled: !studySession.busy; onClicked: window.browseWorkspaceFolder() }
            }
            Label { width: parent.width; visible: window.workspaceFormMode !== "rename"; text: "Used by new terminals. Running terminals keep their current folders."; wrapMode: Text.Wrap; color: "#a5afbe"; font.pixelSize: 12 }
            Label { width: parent.width; text: studySession.error; visible: text.length > 0; wrapMode: Text.Wrap; color: "#e3bf91" }
            Row {
                spacing: 8
                QuietButton { fixedPalette: true; text: "Cancel"; enabled: !studySession.busy; onClicked: createDialog.close() }
                QuietButton {
                    id: submitWorkspace; objectName: "submitWorkspaceButton"; fixedPalette: true
                    text: studySession.busy ? (window.workspaceFormMode === "create" ? "Creating…" : "Saving…") : window.workspaceFormMode === "create" ? "Create" : "Save"
                    enabled: !studySession.busy && (window.workspaceFormMode === "folder" || newName.text.trim().length > 0) && (window.workspaceFormMode === "rename" || newFolder.text.trim().length > 0)
                    onClicked: window.submitWorkspaceForm()
                }
            }
        }
        Component {
            id: folderBrowserFactory
            NativeDialogs.FolderDialog {
                id: browser; objectName: "workspaceFolderBrowser"
                parentWindow: window; title: "Choose workspace folder"; acceptLabel: "Choose folder"
                modality: Qt.WindowModal
                // Automated native-input checks use Qt's fallback, independent of host portals.
                options: testFolderDialogs ? NativeDialogs.FolderDialog.DontUseNativeDialog : 0
                Component.onCompleted: if (testFolderDialogs && "popupType" in browser) browser.popupType = Popup.Item
                onAccepted: {
                    const path = studySession.folderPath(selectedFolder)
                    if (createDialog.visible && path.length > 0) { newFolder.text = path; newFolder.deselect(); newFolder.cursorPosition = newFolder.text.length }
                    Qt.callLater(window.restoreFolderFocus)
                }
                onRejected: Qt.callLater(window.restoreFolderFocus)
            }
        }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Dialog {
        id: help; implicitWidth: 560; implicitHeight: 400; width: Math.min(implicitWidth, window.width - 64); height: Math.min(implicitHeight, window.height - 64); title: "Keyboard shortcuts"; anchors.centerIn: parent; modal: true; standardButtons: Dialog.Close
        contentItem: Label { padding: 18; text: window.chordLabel + "← / →     Previous / next terminal\n" + window.chordLabel + "↑ / ↓     Previous / next workspace\n" + window.chordLabel + "T              New terminal\n" + window.chordLabel + "N             New workspace\n" + window.chordLabel + "O             Overview\n" + window.chordLabel + "R             Cycle ½ → ⅔ → ⅓\n" + window.chordLabel + "F              Full width / restore\n" + window.chordLabel + "/              This help\nEscape             Close Overview or overlay"; lineHeight: 1.6 }
        onClosed: if (!window.overview && window.selected >= 0) window.panel(window.selected).terminal.forceActiveFocus()
    }
    Shortcut { sequence: window.chord + "T"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.newTerminal() }
    Shortcut { sequence: window.chord + "N"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.newWorkspace() }
    Shortcut { sequence: window.chord + "Left"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.navigate(-1) }
    Shortcut { sequence: window.chord + "Right"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.navigate(1) }
    Shortcut { sequence: window.chord + "Up"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.chooseWorkspace(window.workspace - 1) }
    Shortcut { sequence: window.chord + "Down"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.chooseWorkspace(window.workspace + 1) }
    Shortcut { sequence: window.chord + "O"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: window.toggleOverview() }
    Shortcut { sequence: window.chord + "R"; enabled: !window.overview && !createDialog.visible && !window.appearanceOpen; onActivated: window.cycleWidth() }
    Shortcut { sequence: window.chord + "F"; enabled: !window.overview && !createDialog.visible && !window.appearanceOpen; onActivated: window.toggleFull() }
    Shortcut { sequence: window.chord + "/"; enabled: !createDialog.visible && !window.appearanceOpen; onActivated: help.open() }
    Shortcut { sequence: "Escape"; enabled: window.overview && !createDialog.visible && !window.appearanceOpen && !help.visible && !drawer.visible && !appMenu.visible; onActivated: { if (window.dragSlot >= 0) window.cancelDrag(); else window.toggleOverview() } }
    onActiveChanged: if (!active) cancelDrag()
    Component.onCompleted: { window.applyCatalog(studySession.initialCatalog, "initialize"); selected = remembered[0]; Qt.callLater(function() { if (selected >= 0) window.panel(selected).terminal.forceActiveFocus() }) }
}
