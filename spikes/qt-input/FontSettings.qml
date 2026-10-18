import QtQuick
import QtQuick.Controls
Dialog {
    id: popup
    required property var tones
    required property var preferences
    width: Math.min(460, parent.width - 64); padding: 20; modal: true
    closePolicy: Popup.CloseOnEscape
    title: "Terminal text"
    background: Rectangle { color: popup.tones.raised; radius: 12; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: 20; bottomPadding: 8; color: popup.tones.foreground; font.pixelSize: 22 }
    contentItem: Column {
        spacing: 16
        Label { text: "Font"; color: popup.tones.muted; font.pixelSize: 12 }
        Row {
            width: parent.width; spacing: 8
            ChromeButton { objectName: "fontJetBrains"; width: (parent.width - 8)/2; tones: popup.tones; text: "JetBrains Mono"; selected: popup.preferences.current.family === text; onClicked: popup.preferences.preview(text, popup.preferences.current.size) }
            ChromeButton { objectName: "fontFira"; width: (parent.width - 8)/2; tones: popup.tones; text: "Fira Code"; selected: popup.preferences.current.family === text; onClicked: popup.preferences.preview(text, popup.preferences.current.size) }
        }
        Label { text: "Size"; color: popup.tones.muted; font.pixelSize: 12 }
        Row {
            spacing: 16
            ChromeButton { objectName: "fontMinus"; tones: popup.tones; text: "−"; hint: "Decrease terminal text size"; enabled: popup.preferences.current.size > 10; onClicked: popup.preferences.step(-1) }
            Label { width: 60; height: 32; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: popup.preferences.current.size + " pt"; color: popup.tones.foreground }
            ChromeButton { objectName: "fontPlus"; tones: popup.tones; text: "+"; hint: "Increase terminal text size"; enabled: popup.preferences.current.size < 32; onClicked: popup.preferences.step(1) }
        }
        Label { width: parent.width; text: "Preview across this window. Apply saves the font and size for future launches on this platform."; wrapMode: Text.Wrap; color: popup.tones.muted; font.pixelSize: 12 }
        Label { width: parent.width; text: popup.preferences.editError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: popup.tones.waiting; font.pixelSize: 12 }
        Row {
            width: parent.width; spacing: 8
            ChromeButton { objectName: "fontDefaults"; tones: popup.tones; text: "Use dotfile defaults"; onClicked: popup.preferences.useDefaults() }
            ChromeButton { id: cancelButton; tones: popup.tones; text: "Cancel"; onClicked: popup.close() }
            ChromeButton { objectName: "fontApply"; tones: popup.tones; text: "Apply"; onClicked: if (popup.preferences.apply()) popup.close() }
        }
    }
    onOpened: cancelButton.forceActiveFocus()
    onClosed: popup.preferences.cancel()
}
