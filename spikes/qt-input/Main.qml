import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 760; height: 480
    visible: true
    title: "Workbench Toolkit Probe"
    color: "#191d24"
    property int presses: 0
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 18
        Label { text: "Text & input"; font.pixelSize: 26; color: "#edf1fa" }
        Label {
            text: "Toolkit experiment · no terminals or saved sessions"
            color: "#abb5c7"
        }
        Label { text: "Workspace name"; color: "#edf1fa" }
        TextField {
            objectName: "workspaceName"
            Layout.fillWidth: true
            placeholderText: "Try a name, an IME, or clipboard paste"
            font.pixelSize: 20
            selectByMouse: true
            Accessible.name: "Workspace name"
            focus: true
        }
        Button {
            objectName: "newTerminal"
            text: "New terminal"
            Accessible.name: "New terminal"
            onClicked: window.presses++
        }
        Label { text: "Test actions: " + window.presses; color: "#abb5c7" }
        Label {
            text: "café · Ελληνικά · العربية · 日本語 · हिन्दी"
            font.pixelSize: 24; color: "#edf1fa"
            Layout.fillWidth: true
        }
        Item { Layout.fillHeight: true }
        Label { text: "Tab to navigate · use the native input source to compose text"; color: "#abb5c7" }
    }
}
