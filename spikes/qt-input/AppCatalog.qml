import QtQuick
import QtQuick.Controls

// Add applications (docs/apps.md): installed apps on top, then the catalog.
// Choosing an installed app opens it; installing a missing one takes its own
// button, so a stray Return or click never starts a package manager. Sources
// lists the package managers and repositories Band found and sets up Homebrew.
Dialog {
    id: catalog
    required property var host
    readonly property var tones: host.chrome
    readonly property var entries: host.appEntries
    // Until the first check, installed state is unknown and rows would move
    // when it arrives; the list waits for it.
    readonly property bool ready: entries.some(entry => entry.installed !== -1) || !studyApps.checking
    readonly property bool searchFocused: search.activeFocus
    property bool showSources: false
    property string currentId: ""
    // App ID → chosen source, for apps with more than one way to install.
    property var sourceChoice: ({})
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
    signal chosen(string appId, string install, string via)
    signal homebrewRequested()
    signal setupRequested(string appId)
    width: Math.min(960, parent.width - 64); height: parent.height - 64
    padding: 16; modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: catalog.tones.raised; radius: 12; border.color: catalog.tones.border }
    onOpened: { search.clear(); currentId = ""; showSources = false; Qt.callLater(selectFirst); search.forceActiveFocus() }
    onShowSourcesChanged: Qt.callLater(function() { if (showSources) sourcesList.forceActiveFocus(Qt.TabFocusReason); else search.forceActiveFocus() })
    onRowsChanged: {
        const index = rows.findIndex(row => !row.header && row.entry.id === currentId)
        if (index >= 0) results.currentIndex = index; else selectFirst()
    }
    function running(entry) { const revision = host.catalogRevision; return entry && host.runningApp(host.workspace, entry.id) >= 0 }
    function option(entry) {
        const options = entry && entry.options ? entry.options : []
        return options.find(item => item.source === sourceChoice[entry.id]) || options[0] || null
    }
    function choose(entry, source) { let choices = Object.assign({}, sourceChoice); choices[entry.id] = source; sourceChoice = choices }
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
        if (current.entry.installed !== 0 || running(current.entry)) chosen(current.entry.id, "", "")
        else if (actionButton.enabled) actionButton.forceActiveFocus(Qt.TabFocusReason)
    }
    function act() {
        if (!current) return
        const entry = current.entry, picked = option(entry)
        if (entry.installed !== 0 || running(entry)) chosen(entry.id, "", "")
        else if (picked) chosen(entry.id, picked.install, picked.via)
        else if (entry.needsHomebrew) showSources = true
    }
    TextEdit { id: clipboard; visible: false }
    function copy(text) { clipboard.text = text; clipboard.selectAll(); clipboard.copy(); clipboard.text = "" }
    header: Item {
        implicitHeight: 70
        Label { x: 20; y: 14; text: catalog.showSources ? "Sources" : "Add applications"; color: catalog.tones.foreground; font.pixelSize: 22 }
        Label {
            x: 20; y: 45; width: parent.width - 40 - sourcesButton.width; elide: Text.ElideRight; color: catalog.tones.muted; font.pixelSize: 12; textFormat: Text.PlainText
            text: catalog.showSources ? "Where Band installs apps from. Every install runs in a pane you can watch."
                : "Apps open as panes in " + (host.spaces[host.workspace] || "this workspace") + ". The list comes from awesome-tuis."
        }
        ChromeButton {
            id: sourcesButton; objectName: "appSourcesButton"; anchors.right: parent.right; anchors.rightMargin: 16; y: 14
            tones: catalog.tones; text: catalog.showSources ? "Back to apps" : "Sources"; selected: catalog.showSources
            hint: catalog.showSources ? "Back to the app list" : "Package managers and repositories"
            onClicked: catalog.showSources = !catalog.showSources
        }
    }
    contentItem: Item {
        Item {
            anchors.fill: parent; visible: !catalog.showSources
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
                        id: appItem
                        objectName: "appCatalogRow" + (row.modelData.header ? "" : row.modelData.entry.id)
                        visible: !row.modelData.header; anchors.fill: parent; focusPolicy: Qt.NoFocus
                        highlighted: results.currentIndex === row.index
                        readonly property var entry: row.modelData.header ? ({}) : row.modelData.entry
                        readonly property var picked: catalog.option(entry)
                        readonly property string status: catalog.running(entry) ? "Running" : entry.installed === 0 ? (picked ? "Install · " + picked.via : entry.needsHomebrew ? "Needs Homebrew" : "Not available") : ""
                        Accessible.name: (entry.name || "") + (status ? ", " + status : "")
                        onClicked: { catalog.select(row.index); results.forceActiveFocus() }
                        onDoubleClicked: { catalog.select(row.index); catalog.activate() }
                        contentItem: Item {
                            Text { x: 8; y: 7; width: parent.width - side.width - 32; elide: Text.ElideRight; text: appItem.entry.name || ""; textFormat: Text.PlainText; font.pixelSize: 14; color: catalog.tones.foreground }
                            Text { x: 8; y: 28; width: parent.width - side.width - 32; elide: Text.ElideRight; text: appItem.entry.description || appItem.entry.command || ""; textFormat: Text.PlainText; font.pixelSize: 12; color: catalog.tones.muted }
                            Column {
                                id: side; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                                Text { anchors.right: parent.right; text: appItem.status; font.pixelSize: 12; color: appItem.status === "Running" ? catalog.tones.ready : appItem.status.startsWith("Install") ? catalog.tones.accent : catalog.tones.muted }
                                Text { anchors.right: parent.right; text: appItem.entry.section || (appItem.entry.builtin === false ? "config.conf" : ""); font.pixelSize: 11; color: catalog.tones.muted }
                            }
                        }
                        background: Rectangle { radius: 6; color: parent.highlighted ? catalog.tones.selected : parent.hovered ? catalog.tones.hover : "transparent"; border.width: parent.highlighted && results.activeFocus ? 1 : 0; border.color: catalog.tones.accent }
                    }
                }
                Label {
                    anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                    visible: catalog.matchCount === 0; color: catalog.tones.muted
                    text: !catalog.ready ? "Checking which apps are installed…" : "No matching apps"
                }
            }
            Rectangle {
                id: detail; y: parent.height - height; width: parent.width; height: 84 + (sourceRow.visible ? 36 : 0); radius: 8
                color: catalog.tones.background; border.color: catalog.tones.border; visible: catalog.current !== null
                readonly property var entry: catalog.current ? catalog.current.entry : ({})
                readonly property bool missing: entry.installed === 0 && !catalog.running(entry)
                readonly property var picked: catalog.option(entry)
                Column {
                    x: 14; y: 12; width: parent.width - actionButton.width - (setupButton.visible ? setupButton.width + 8 : 0) - 42; spacing: 4
                    Text { width: parent.width; elide: Text.ElideRight; text: (detail.entry.name || "") + (detail.entry.description ? " — " + detail.entry.description : ""); textFormat: Text.PlainText; color: catalog.tones.foreground; font.pixelSize: 13 }
                    Text {
                        objectName: "appCatalogDetail"; width: parent.width; elide: Text.ElideMiddle; textFormat: Text.PlainText; font.family: "monospace"; font.pixelSize: 12; color: catalog.tones.muted
                        text: detail.missing ? (detail.picked ? detail.picked.install : "") : (detail.entry.command || "")
                    }
                    Text {
                        width: parent.width; elide: Text.ElideRight; textFormat: Text.PlainText; font.pixelSize: 12; color: detail.missing && !detail.picked ? catalog.tones.waiting : catalog.tones.muted
                        text: detail.missing ? (detail.picked ? "Installs in a pane you can watch" + (detail.entry.setup ? ", then runs its setup" : "") + "; " + (detail.entry.name || "") + " opens when it is done." : (detail.entry.hint || "")) : catalog.running(detail.entry) ? "Running in this workspace." : detail.entry.setup ? "Setup: " + detail.entry.setup : ""
                    }
                    // More than one way to install: pick the source.
                    Row {
                        id: sourceRow; spacing: 4; topPadding: 6; visible: detail.missing && (detail.entry.options || []).length > 1
                        Label { height: 28; verticalAlignment: Text.AlignVCenter; rightPadding: 4; text: "Install from"; color: catalog.tones.muted; font.pixelSize: 12 }
                        Repeater {
                            model: sourceRow.visible ? detail.entry.options : []
                            ChromeButton {
                                required property var modelData
                                objectName: "appSourceChoice_" + modelData.source
                                height: 28; tones: catalog.tones; text: modelData.via
                                selected: detail.picked && detail.picked.source === modelData.source
                                hint: modelData.install
                                onClicked: catalog.choose(detail.entry, modelData.source)
                            }
                        }
                    }
                }
                // Setup from config.conf runs after installing; this runs it again.
                ChromeButton {
                    id: setupButton; objectName: "appCatalogSetup"
                    anchors.right: actionButton.left; anchors.rightMargin: 8; y: 24; height: 36
                    visible: catalog.current !== null && !detail.missing && !!detail.entry.setup; tones: catalog.tones
                    enabled: host.canCreateTerminal; text: "Run setup"; hint: detail.entry.setup || ""
                    onClicked: if (catalog.current) catalog.setupRequested(catalog.current.entry.id)
                }
                ChromeButton {
                    id: actionButton; objectName: "appCatalogAction"
                    anchors.right: parent.right; anchors.rightMargin: 14; y: 24; height: 36
                    tones: catalog.tones; primary: true
                    enabled: catalog.current !== null && (!detail.missing || !!detail.picked || !!detail.entry.needsHomebrew) && host.canCreateTerminal
                    text: catalog.running(detail.entry) ? "Go to" : !detail.missing ? "Open" : detail.picked ? (sourceRow.visible ? "Install" : "Install with " + detail.picked.via) : detail.entry.needsHomebrew ? "Set up Homebrew…" : "Not available"
                    KeyNavigation.tab: doneButton; KeyNavigation.backtab: results
                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()
                    onClicked: catalog.act()
                }
            }
        }
        // Sources: package managers and repositories, and Homebrew's setup.
        ListView {
            id: sourcesList; objectName: "appSources"; anchors.fill: parent; visible: catalog.showSources
            // A count, not the list: rows survive each check, so the pointer's
            // target stays put.
            clip: true; spacing: 8; model: studyApps.sources.length; activeFocusOnTab: true; boundsBehavior: Flickable.StopAtBounds
            Accessible.name: "Sources"
            delegate: Rectangle {
                id: sourceRowItem
                required property int index
                readonly property var modelData: studyApps.sources[index] || ({id: "", name: "", detail: "", found: false, enabled: false})
                readonly property bool brew: modelData.id === "brew"
                readonly property var homebrew: studyApps.homebrew
                width: sourcesList.width; height: sourceBody.implicitHeight + 24; radius: 8
                color: catalog.tones.background; border.color: catalog.tones.border
                Column {
                    id: sourceBody; x: 14; y: 12; width: parent.width - toggle.width - 42; spacing: 6
                    Row {
                        spacing: 8
                        Text { text: sourceRowItem.modelData.name; textFormat: Text.PlainText; color: catalog.tones.foreground; font.pixelSize: 14 }
                        Text {
                            anchors.baseline: parent.children[0].baseline; font.pixelSize: 12
                            text: sourceRowItem.modelData.found ? (sourceRowItem.modelData.enabled ? "On" : "Off") : sourceRowItem.brew && sourceRowItem.homebrew.path ? "Not loaded" : "Not available"
                            color: sourceRowItem.modelData.found && sourceRowItem.modelData.enabled ? catalog.tones.ready : catalog.tones.muted
                        }
                    }
                    Text { width: parent.width; wrapMode: Text.Wrap; text: sourceRowItem.modelData.detail; textFormat: Text.PlainText; color: catalog.tones.muted; font.pixelSize: 12 }
                    // Installed but not on PATH: the line for the shell profile.
                    Row {
                        visible: sourceRowItem.brew && !sourceRowItem.modelData.found && !!sourceRowItem.homebrew.shellenv; spacing: 8
                        Text { width: sourceBody.width - copyButton.width - 8; anchors.verticalCenter: parent.verticalCenter; elide: Text.ElideMiddle; text: sourceRowItem.homebrew.shellenv || ""; textFormat: Text.PlainText; font.family: "monospace"; font.pixelSize: 12; color: catalog.tones.foreground }
                        ChromeButton { id: copyButton; objectName: sourceRowItem.brew ? "homebrewCopyShellenv" : ""; height: 28; tones: catalog.tones; text: "Copy"; hint: "Copy this line, add it to your shell profile, then open the catalog again"; onClicked: catalog.copy(sourceRowItem.homebrew.shellenv) }
                    }
                    Text {
                        visible: sourceRowItem.brew && !sourceRowItem.modelData.found && !!sourceRowItem.homebrew.shellenv; width: parent.width; wrapMode: Text.Wrap; color: catalog.tones.muted; font.pixelSize: 12
                        text: "Add this line to your shell profile, then open the catalog again."
                    }
                    // Not installed: Homebrew's own installer, in a pane.
                    Column {
                        visible: sourceRowItem.brew && !sourceRowItem.modelData.found && !sourceRowItem.homebrew.path && sourceRowItem.homebrew.checked; width: parent.width; spacing: 6
                        Text { width: parent.width; elide: Text.ElideMiddle; text: sourceRowItem.homebrew.install || ""; textFormat: Text.PlainText; font.family: "monospace"; font.pixelSize: 12; color: catalog.tones.foreground }
                        Text {
                            width: parent.width; wrapMode: Text.Wrap; color: catalog.tones.muted; font.pixelSize: 12
                            text: "Runs Homebrew's official installer from GitHub in a pane, where it asks for your password and confirmation" + (Qt.platform.os === "osx" ? "." : ". On Linux it needs curl, git and a compiler, and it prints a line to add to your shell profile.")
                        }
                        ChromeButton {
                            objectName: sourceRowItem.brew ? "homebrewInstall" : ""; height: 32; tones: catalog.tones; primary: true; text: "Install Homebrew"
                            enabled: host.canCreateTerminal
                            onClicked: catalog.homebrewRequested()
                        }
                    }
                }
                ChromeButton {
                    id: toggle; objectName: "appSourceToggle_" + sourceRowItem.modelData.id
                    anchors.right: parent.right; anchors.rightMargin: 14; y: 12; height: 30
                    visible: sourceRowItem.modelData.found; tones: catalog.tones
                    text: sourceRowItem.modelData.enabled ? "Turn off" : "Turn on"
                    hint: (sourceRowItem.modelData.enabled ? "Stop offering " : "Offer ") + sourceRowItem.modelData.name + " for installs"
                    onClicked: studyApps.setSourceEnabled(sourceRowItem.modelData.id, !sourceRowItem.modelData.enabled)
                }
            }
            Label {
                anchors.centerIn: parent; visible: sourcesList.count === 0; color: catalog.tones.muted
                text: studyApps.checking ? "Checking package managers…" : "No package managers found."
            }
        }
    }
    footer: Item {
        implicitHeight: 48
        Label {
            x: 20; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 12; color: catalog.tones.muted
            visible: catalog.showSources; text: "Choices are saved for this computer."
        }
        Row {
            x: 20; anchors.verticalCenter: parent.verticalCenter; spacing: 6; visible: !catalog.showSources
            Accessible.role: Accessible.StaticText; Accessible.name: "Up and Down choose, Enter opens, Escape closes. " + appCount.text
            KeyChips { keys: "Up"; tones: catalog.tones } KeyChips { keys: "Down"; tones: catalog.tones }
            Text { text: "choose"; font.pixelSize: 12; color: catalog.tones.muted; rightPadding: 8 }
            KeyChips { keys: "Enter"; tones: catalog.tones }
            Text { text: "open"; font.pixelSize: 12; color: catalog.tones.muted; rightPadding: 8 }
            KeyChips { keys: "Esc"; tones: catalog.tones }
            Text { text: "close"; font.pixelSize: 12; color: catalog.tones.muted; rightPadding: 8 }
            Text { id: appCount; text: catalog.matchCount + (catalog.matchCount === 1 ? " app" : " apps"); font.pixelSize: 12; color: catalog.tones.muted }
        }
        ChromeButton { id: doneButton; Keys.onReturnPressed: catalog.close(); Keys.onEnterPressed: catalog.close(); KeyNavigation.tab: search; anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; text: "Done"; tones: catalog.tones; onClicked: catalog.close() }
    }
}
