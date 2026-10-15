import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Workbench.Probe
ApplicationWindow {
    width: 920; height: 640; visible: true
    title: "Workbench Terminal Probe"
    color: "#191d24"
    property bool disposable: false
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.margins: 12
            Label { text: terminal.status; color: "#c9d2e3"; Layout.fillWidth: true }
            Button { text: "Copy"; onClicked: terminal.copy() }
            Button { text: "Paste"; onClicked: terminal.paste() }
        }
        TerminalItem { id: terminal; objectName: "terminal"; Layout.fillWidth: true; Layout.fillHeight: true; focus: true }
        Label { text: disposable ? "Disposable test shell · closing the probe stops this shell" : "Terminal host experiment · closing the window detaches"; color: "#abb5c7"; Layout.margins: 10 }
    }
}
