import QtQuick
import QtQuick.Controls

TextField {
    id: field
    required property var tones
    signal escapePressed()
    signal stepRequested(int direction)
    height: 32; selectByMouse: true; maximumLength: 256
    color: tones.foreground; placeholderTextColor: tones.muted
    selectionColor: tones.accent; selectedTextColor: tones.background
    leftPadding: 10; rightPadding: 32; font.pixelSize: 13
    background: Rectangle { color: field.tones.background; radius: 6; border.color: field.activeFocus ? field.tones.accent : field.tones.border }
    ChromeButton {
        objectName: field.objectName + "Clear"
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        tones: field.tones; iconName: "close"; hint: "Clear search"; visible: field.text.length > 0
        onClicked: { field.clear(); field.forceActiveFocus() }
    }
    Keys.onEscapePressed: event => { if (!inputMethodComposing) { escapePressed(); event.accepted = true } else event.accepted = false }
    Keys.onDownPressed: event => { if (!inputMethodComposing) { stepRequested(1); event.accepted = true } else event.accepted = false }
    Keys.onUpPressed: event => { if (!inputMethodComposing) { stepRequested(-1); event.accepted = true } else event.accepted = false }
}
