import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Workbench.Probe
ApplicationWindow {
    id: window
    width: 1120; height: 640; visible: true
    title: "Workbench Terminal Probe"
    color: "#191d24"
    property bool disposable: false
    property var selectedTerminal: terminal
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Accessible.role: Accessible.Pane
        Accessible.name: "Two-terminal workspace"
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 12
            Label { text: "Two-terminal input probe"; color: "#c9d2e3"; Layout.fillWidth: true }
            Button { objectName: "copyButton"; text: "Copy"; onClicked: selectedTerminal.copy() }
            Button { objectName: "pasteButton"; text: "Paste"; onClicked: selectedTerminal.paste() }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 1
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; spacing: 0
                Label { text: "A · " + terminal.status; color: terminal.activeFocus ? "#a8c7fa" : "#abb5c7"; Layout.margins: 8 }
                TerminalItem { id: terminal; objectName: "terminal"; Layout.fillWidth: true; Layout.fillHeight: true; focus: true
                    Accessible.role: Accessible.Terminal
                    Accessible.name: "Terminal A"
                    onActiveFocusChanged: if (activeFocus) window.selectedTerminal = terminal
                }
            }
            Rectangle { color: "#405575"; Layout.preferredWidth: 1; Layout.fillHeight: true }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.preferredWidth: 1; spacing: 0
                Label { text: "B · " + peer.status; color: peer.activeFocus ? "#a8c7fa" : "#abb5c7"; Layout.margins: 8 }
                TerminalItem { id: peer; objectName: "peer"; Layout.fillWidth: true; Layout.fillHeight: true
                    Accessible.role: Accessible.Terminal
                    Accessible.name: "Terminal B"
                    onActiveFocusChanged: if (activeFocus) window.selectedTerminal = peer
                }
            }
        }
        Label { text: disposable ? "Disposable shells · closing stops both shells" : "Two-terminal input experiment · closing detaches"; color: "#abb5c7"; Layout.margins: 10 }
    }
}
