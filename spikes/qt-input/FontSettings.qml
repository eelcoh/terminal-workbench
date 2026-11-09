import QtQuick
import QtQuick.Controls
import Band.Design
Dialog {
    id: popup
    required property var tones
    required property var preferences
    width: Math.min(460, parent.width - 64); padding: Design.space2xl; modal: true
    closePolicy: Popup.CloseOnEscape
    title: "Terminal text"
    background: Rectangle { color: popup.tones.raised; radius: Design.radiusXl; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: Design.space2xl; bottomPadding: Design.spaceM; color: popup.tones.foreground; font.pixelSize: Design.textTitle }
    contentItem: Column {
        spacing: Design.spaceXl
        Label { text: "Font"; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        Row {
            width: parent.width; spacing: Design.spaceM
            ChromeButton { objectName: "fontJetBrains"; width: (parent.width - 8)/2; tones: popup.tones; text: "JetBrains Mono"; selected: popup.preferences.current.family === text; onClicked: popup.preferences.preview(text, popup.preferences.current.size) }
            ChromeButton { objectName: "fontFira"; width: (parent.width - 8)/2; tones: popup.tones; text: "Fira Code"; selected: popup.preferences.current.family === text; onClicked: popup.preferences.preview(text, popup.preferences.current.size) }
        }
        Label { text: "Size"; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        Row {
            spacing: Design.spaceXl
            ChromeButton { objectName: "fontMinus"; tones: popup.tones; text: "−"; hint: "Decrease terminal text size"; enabled: popup.preferences.current.size > 10; onClicked: popup.preferences.step(-1) }
            Label { width: 60; height: 32; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: popup.preferences.current.size + " pt"; color: popup.tones.foreground }
            ChromeButton { objectName: "fontPlus"; tones: popup.tones; text: "+"; hint: "Increase terminal text size"; enabled: popup.preferences.current.size < 32; onClicked: popup.preferences.step(1) }
        }
        Label { width: parent.width; text: "Preview across this window. Apply saves the font and size for future launches on this platform."; wrapMode: Text.Wrap; color: popup.tones.muted; font.pixelSize: Design.textSmall }
        Label { width: parent.width; visible: text.length > 0; text: popup.preferences.editError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: popup.tones.waiting; font.pixelSize: Design.textSmall }
    }
    // The reset on the left; the decision on the right, Apply highlighted.
    footer: Item {
        implicitHeight: 56
        ChromeButton { objectName: "fontDefaults"; x: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; tones: popup.tones; text: "Use dotfile defaults"; onClicked: popup.preferences.useDefaults() }
        Row {
            anchors.right: parent.right; anchors.rightMargin: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceM
            ChromeButton { id: cancelButton; tones: popup.tones; text: "Cancel"; onClicked: popup.close() }
            ChromeButton { objectName: "fontApply"; tones: popup.tones; primary: true; text: "Apply"; onClicked: if (popup.preferences.apply()) popup.close() }
        }
    }
    onOpened: cancelButton.forceActiveFocus()
    onClosed: popup.preferences.cancel()
}
