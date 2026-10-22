import QtQuick
import QtQuick.Controls
Dialog {
    id: popup
    required property var tones
    required property var inspector
    required property var pane
    property string contextLabel: ""
    property bool expanded: false
    property bool copied: false
    readonly property var report: inspector.report
    readonly property bool available: report.state === "available"
    readonly property bool checking: report.state === "checking"
    readonly property var capabilities: report.capabilities || []
    readonly property string ownership: !pane ? "Unavailable" : pane.terminal.ready ? "Attached here" : pane.terminal.attachment === "elsewhere" ? "Attached in another window" : pane.lifecycle.state === "stopped" ? "Stopped" : pane.lifecycle.state === "running" ? "Running · disconnected here" : "Unavailable"
    onReportChanged: copied = false
    width: Math.min(580, parent.width - 64); height: Math.min(590, parent.height - 64)
    padding: 20; modal: true; focus: true; closePolicy: Popup.CloseOnEscape
    title: "Terminal details"
    background: Rectangle { color: popup.tones.raised; radius: 12; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: 20; bottomPadding: 8; font.pixelSize: 22; color: popup.tones.foreground }
    contentItem: Flickable {
        contentWidth: width; contentHeight: fields.height; clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: fields; width: parent.width; spacing: 14
            Label { text: popup.contextLabel; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: 12; color: popup.tones.muted }
            Label { text: popup.ownership; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 17; color: popup.tones.foreground }
            Label { text: popup.pane ? popup.pane.lifecycle.message || "Exit status unavailable" : "Terminal no longer exists"; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.muted }
            Label { text: popup.report.message || "Runtime details unavailable"; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.foreground }
            Column {
                width: parent.width; spacing: 7; visible: popup.available
                Label { text: "Runtime PID: " + (popup.report.runtime_pid || "Unavailable"); color: popup.tones.foreground; font.pixelSize: 13 }
                Repeater {
                    model: [{name: "Snapshot reattachment", capability: "snapshot"}, {name: "Exit records", capability: "terminal-lifecycle"}, {name: "Extended agent metadata", capability: "agent-watch-v3"}]
                    Label { required property var modelData; text: modelData.name + ": " + (popup.capabilities.indexOf(modelData.capability) >= 0 ? "Advertised" : "Not advertised"); color: popup.tones.foreground; font.pixelSize: 13 }
                }
                Label { width: parent.width; text: "Runtime launch path: " + (popup.report.runtime_binary || "Unavailable"); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: popup.tones.muted; font.pixelSize: 12 }
                Label { width: parent.width; text: "Helper launch path: " + (popup.report.agent_bin || "Unavailable"); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: popup.tones.muted; font.pixelSize: 12 }
            }
            Label { text: "Existing terminals keep their original runtime and launch helpers. Rebuilding or opening a newer viewer does not upgrade a running shell. Create a new terminal to use the current runtime; keep existing work running."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.muted }
            Row {
                spacing: 8
                ChromeButton { objectName: "terminalInfoCopy"; tones: popup.tones; enabled: !popup.checking && (popup.report.identity || "").length > 0; text: popup.copied ? "Copied" : "Copy diagnostics"; onClicked: popup.copied = popup.inspector.copyDiagnostics() }
                ChromeButton { objectName: "terminalInfoExpand"; tones: popup.tones; text: popup.expanded ? "Hide report" : "Show report"; onClicked: popup.expanded = !popup.expanded }
            }
            TextEdit { objectName: "terminalInfoReport"; width: parent.width; height: contentHeight; visible: popup.expanded; text: popup.inspector.diagnostics; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; color: popup.tones.foreground; font.pixelSize: 12 }
            Label { text: "Diagnostics include terminal identity, runtime capabilities and local installation paths. Terminal text, commands, environment and project metadata are excluded."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 12; color: popup.tones.muted }
        }
    }
    footer: Item {
        implicitHeight: 52
        ChromeButton { objectName: "terminalInfoRefresh"; x: 20; tones: popup.tones; text: popup.checking ? "Checking…" : "Refresh"; enabled: !popup.checking && popup.pane !== null; onClicked: popup.refreshRequested() }
        ChromeButton { id: done; objectName: "terminalInfoClose"; anchors.right: parent.right; anchors.rightMargin: 20; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    signal refreshRequested()
    onOpened: { copied = false; expanded = false; done.forceActiveFocus() }
}
