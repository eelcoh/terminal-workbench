import QtQuick
import QtQuick.Controls

// Add applications (docs/apps.md): installed apps on top, then the catalog.
// Choosing an installed app opens it; installing a missing one takes its own
// button, so a stray Return or click never starts a package manager.
Dialog {
    id: catalog
    required property var host
    readonly property var tones: host.chrome
    readonly property var entries: host.appEntries
    // Until the first check, installed state is unknown and rows would move
    // when it arrives; the list waits for it.
    readonly property bool ready: entries.some(entry => entry.installed !== -1) || !studyApps.checking
    readonly property bool searchFocused: search.activeFocus
    property string currentId: ""
    readonly property var current: rows.find(row => !row.header && row.entry.id === currentId) || null
    readonly property var rows: {
        if (!ready) return []
        const words = search.text.trim().toLowerCase().split(/\s+/).filter(word => word.length)
        const matches = entries.filter(entry => words.every(word => (entry.name + " " + entry.command + " " + entry.description + " " + (entry.section || "")).toLowerCase().indexOf(word) >= 0))
        const installed = matches.filter(entry => entry.installed !== 0)
        // Curated apps lead the rest; then by name.
        const available = matches.filter(entry => entry.installed === 0).sort((a, b) => (b.featured - a.featured) || a.name.toLowerCase().localeCompare(b.name.toLowerCase()))
        let result = []
        if (installed.length) result.push({header: true, title: "Installed"}, ...installed.map(entry => ({header: false, entry: entry})))
        if (available.length) result.push({header: true, title: "Available"}, ...available.map(entry => ({header: false, entry: entry})))
        return result
    }
    readonly property int matchCount: rows.filter(row => !row.header).length
    signal chosen(string appId)
    width: Math.min(960, parent.width - 64); height: parent.height - 64
    padding: 16; modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: catalog.tones.raised; radius: 12; border.color: catalog.tones.border }
    onOpened: { search.clear(); currentId = ""; Qt.callLater(selectFirst); search.forceActiveFocus() }
    onRowsChanged: {
        const index = rows.findIndex(row => !row.header && row.entry.id === currentId)
        if (index >= 0) results.currentIndex = index; else selectFirst()
    }
    function running(entry) { const revision = host.catalogRevision; return entry && host.runningApp(host.workspace, entry.id) >= 0 }
    function selectFirst() { const index = rows.findIndex(row => !row.header); select(index) }
    function select(index) {
        if (index < 0 || index >= rows.length || rows[index].header) { results.currentIndex = -1; currentId = ""; return }
        results.currentIndex = index; currentId = rows[index].entry.id
        results.positionViewAtIndex(index, ListView.Contain)
    }
    function step(delta) {
        if (!matchCount) return
        let index = results.currentIndex
        do index = (index + delta + rows.length) % rows.length; while (rows[index].header)
        select(index)
    }
    // Return: open an installed app; for a missing one, move to its Install
    // button, which asks for one more Return.
    function activate() {
        if (!current) return
        if (current.entry.installed !== 0 || running(current.entry)) chosen(current.entry.id)
        else if (current.entry.install) actionButton.forceActiveFocus(Qt.TabFocusReason)
    }
    header: Item {
        implicitHeight: 70
        Label { x: 20; y: 14; text: "Add applications"; color: catalog.tones.foreground; font.pixelSize: 22 }
        Label {
            x: 20; y: 45; width: parent.width - 40 - trial.width; elide: Text.ElideRight; color: catalog.tones.muted; font.pixelSize: 12; textFormat: Text.PlainText
            text: "Apps open as panes in " + (host.spaces[host.workspace] || "this workspace") + ". The list comes from awesome-tuis."
        }
        // Temporary: compare the two sidebar layouts (docs/apps.md).
        Row {
            id: trial; anchors.right: parent.right; anchors.rightMargin: 16; y: 14; spacing: 4
            Label { height: 32; verticalAlignment: Text.AlignVCenter; text: "Sidebar trial"; color: catalog.tones.muted; font.pixelSize: 12 }
            ChromeButton { objectName: "appsTrialList"; tones: catalog.tones; text: "Apps list"; selected: host.appsSidebar === "list"; hint: "Running apps from every workspace in the Apps panel"; onClicked: host.appsSidebar = "list" }
            ChromeButton { objectName: "appsTrialWorkspaces"; tones: catalog.tones; text: "On workspaces"; selected: host.appsSidebar === "workspaces"; hint: "Running apps shown on each workspace row"; onClicked: host.appsSidebar = "workspaces" }
        }
    }
    contentItem: Item {
        ContextSearch {
            id: search; objectName: "appCatalogSearch"; width: parent.width; height: 40; tones: catalog.tones
            placeholderText: "Find apps by name, purpose or category…"; Accessible.name: "Find apps"
            KeyNavigation.tab: results
            onStepRequested: direction => catalog.step(direction)
            onAccepted: if (!inputMethodComposing) catalog.activate()
            onEscapePressed: { if (text.length) clear(); else catalog.close() }
        }
        ListView {
            id: results; objectName: "appCatalogResults"; activeFocusOnTab: true; keyNavigationEnabled: false
            y: search.height + 10; width: parent.width; height: detail.y - y - 10
            clip: true; model: catalog.rows; spacing: 2; currentIndex: -1; boundsBehavior: Flickable.StopAtBounds
            Accessible.name: "Apps"
            ScrollBar.vertical: ScrollBar {}
            KeyNavigation.tab: actionButton.enabled ? actionButton : doneButton; KeyNavigation.backtab: search
            Keys.onUpPressed: catalog.step(-1)
            Keys.onDownPressed: catalog.step(1)
            Keys.onReturnPressed: catalog.activate()
            Keys.onEnterPressed: catalog.activate()
            delegate: Item {
                id: row
                required property var modelData
                required property int index
                width: results.width; height: modelData.header ? 34 : 52
                Label {
                    visible: row.modelData.header; x: 8; anchors.bottom: parent.bottom; anchors.bottomMargin: 6
                    text: row.modelData.header ? row.modelData.title : ""; color: catalog.tones.muted; font.pixelSize: 12; font.weight: Font.DemiBold
                }
                ItemDelegate {
                    objectName: "appCatalogRow" + (row.modelData.header ? "" : row.modelData.entry.id)
                    visible: !row.modelData.header; anchors.fill: parent; focusPolicy: Qt.NoFocus
                    highlighted: results.currentIndex === row.index
                    readonly property var entry: row.modelData.header ? ({}) : row.modelData.entry
                    readonly property string status: catalog.running(entry) ? "Running" : entry.installed === 0 ? (entry.install ? "Install · " + entry.via : "Not available") : ""
                    Accessible.name: (entry.name || "") + (status ? ", " + status : "")
                    onClicked: { catalog.select(row.index); results.forceActiveFocus() }
                    onDoubleClicked: { catalog.select(row.index); catalog.activate() }
                    contentItem: Item {
                        Text { x: 8; y: 7; width: parent.width - side.width - 32; elide: Text.ElideRight; text: parent.parent.entry.name || ""; textFormat: Text.PlainText; font.pixelSize: 14; color: catalog.tones.foreground }
                        Text { x: 8; y: 28; width: parent.width - side.width - 32; elide: Text.ElideRight; text: parent.parent.entry.description || parent.parent.entry.command || ""; textFormat: Text.PlainText; font.pixelSize: 12; color: catalog.tones.muted }
                        Column {
                            id: side; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                            Text { anchors.right: parent.right; text: parent.parent.parent.status; font.pixelSize: 12; color: parent.parent.parent.status === "Running" ? catalog.tones.ready : parent.parent.parent.status.startsWith("Install") ? catalog.tones.accent : catalog.tones.muted }
                            Text { anchors.right: parent.right; text: parent.parent.parent.entry.section || (parent.parent.parent.entry.builtin === false ? "config.conf" : ""); font.pixelSize: 11; color: catalog.tones.muted }
                        }
                    }
                    background: Rectangle { radius: 6; color: parent.highlighted || parent.hovered ? catalog.tones.hover : "transparent"; border.width: parent.highlighted && results.activeFocus ? 1 : 0; border.color: catalog.tones.accent }
                }
            }
            Label {
                anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                visible: catalog.matchCount === 0; color: catalog.tones.muted
                text: !catalog.ready ? "Checking which apps are installed…" : "No matching apps"
            }
        }
        Rectangle {
            id: detail; y: parent.height - height; width: parent.width; height: 84; radius: 8
            color: catalog.tones.background; border.color: catalog.tones.border; visible: catalog.current !== null
            readonly property var entry: catalog.current ? catalog.current.entry : ({})
            readonly property bool missing: entry.installed === 0 && !catalog.running(entry)
            Column {
                x: 14; y: 12; width: parent.width - actionButton.width - 42; spacing: 4
                Text { width: parent.width; elide: Text.ElideRight; text: (detail.entry.name || "") + (detail.entry.description ? " — " + detail.entry.description : ""); textFormat: Text.PlainText; color: catalog.tones.foreground; font.pixelSize: 13 }
                Text {
                    objectName: "appCatalogDetail"; width: parent.width; elide: Text.ElideMiddle; textFormat: Text.PlainText; font.family: "monospace"; font.pixelSize: 12; color: catalog.tones.muted
                    text: detail.missing ? (detail.entry.install || "") : (detail.entry.command || "")
                }
                Text {
                    width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; font.pixelSize: 12; color: detail.missing && !detail.entry.install ? catalog.tones.waiting : catalog.tones.muted
                    text: detail.missing ? (detail.entry.install ? "Installs in a pane you can watch; " + (detail.entry.name || "") + " opens when it is done." : (detail.entry.hint || "")) : catalog.running(detail.entry) ? "Running in this workspace." : ""
                }
            }
            ChromeButton {
                id: actionButton; objectName: "appCatalogAction"
                anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; height: 36
                tones: catalog.tones; selected: true
                enabled: catalog.current !== null && (!detail.missing || !!detail.entry.install) && host.canCreateTerminal
                text: catalog.running(detail.entry) ? "Go to" : detail.missing ? (detail.entry.install ? "Install with " + detail.entry.via : "Not available") : "Open"
                KeyNavigation.tab: doneButton; KeyNavigation.backtab: results
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                onClicked: if (catalog.current) catalog.chosen(catalog.current.entry.id)
            }
        }
    }
    footer: Item {
        implicitHeight: 48
        Label { x: 20; anchors.verticalCenter: parent.verticalCenter; text: "↑ ↓ choose · Enter open · Esc close · " + catalog.matchCount + (catalog.matchCount === 1 ? " app" : " apps"); font.pixelSize: 12; color: catalog.tones.muted }
        ChromeButton { id: doneButton; Keys.onReturnPressed: catalog.close(); Keys.onEnterPressed: catalog.close(); KeyNavigation.tab: search; anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; text: "Done"; tones: catalog.tones; onClicked: catalog.close() }
    }
}
