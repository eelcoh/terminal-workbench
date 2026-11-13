import QtQuick
import QtQuick.Controls
import Band.Design

TextField {
    id: field
    required property var tones
    signal escapePressed()
    signal stepRequested(int direction)
    height: 32; selectByMouse: true; maximumLength: 256
    color: tones.foreground; placeholderTextColor: tones.muted
    selectionColor: tones.accent; selectedTextColor: tones.background
    leftPadding: Design.spaceL; rightPadding: 32; font.pixelSize: Design.textBody
    // A soft wash like the buttons', no border at rest; the accent line marks focus.
    background: Rectangle {
        color: Design.withAlpha(field.tones.foreground, field.activeFocus ? 0.04 : field.hovered ? 0.09 : 0.06); radius: Design.radiusM
        border.width: field.activeFocus ? Design.line : 0; border.color: field.tones.accent
        Behavior on color { ColorAnimation { duration: Design.fade } }
    }
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
