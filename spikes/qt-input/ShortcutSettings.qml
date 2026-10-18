import QtQuick
import QtQuick.Controls
Dialog {
    id: popup
    required property var tones
    required property var preferences
    required property var actions
    property string recording: ""
    property string selectedAction: "commands"
    readonly property bool recordingActive: recording.length > 0
    title: "Shortcut settings"
    width: Math.min(600, parent.width - 64); height: Math.min(570, parent.height - 64)
    padding: 20; modal: true
    closePolicy: recordingActive ? Popup.NoAutoClose : Popup.CloseOnEscape
    background: Rectangle { color: popup.tones.raised; radius: 12; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: 20; bottomPadding: 8; color: popup.tones.foreground; font.pixelSize: 22 }
    function startRecording(id) { selectedAction = id; recording = id; recorder.contentItem.forceActiveFocus() }
    contentItem: Column {
        spacing: 12
        Label { width: parent.width; text: "Record a shortcut, clear it, or restore its dotfile default. Apply saves for this platform."; wrapMode: Text.Wrap; color: popup.tones.muted; font.pixelSize: 12 }
        ListView {
            id: list; objectName: "shortcutSettingsList"
            width: parent.width; height: Math.max(90, parent.height - 130); clip: true; spacing: 4
            model: popup.actions.filter(entry => popup.preferences.editable(entry.id))
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: option; required property var modelData
                objectName: "shortcutAction_" + modelData.id
                width: list.width; height: 40
                Accessible.name: modelData.title + ", " + (popup.preferences.draftLabels[modelData.id] || "Unbound")
                background: Rectangle { radius: 5; color: popup.selectedAction === option.modelData.id ? popup.tones.selected : option.hovered ? popup.tones.hover : "transparent"; border.width: option.visualFocus ? 1 : 0; border.color: popup.tones.accent }
                contentItem: Row {
                    spacing: 8
                    Label { width: parent.width * .59; text: option.modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: popup.tones.foreground; anchors.verticalCenter: parent.verticalCenter }
                    Label { width: parent.width * .38; text: popup.preferences.draftLabels[option.modelData.id] || "Unbound"; color: popup.tones.muted; elide: Text.ElideRight; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter }
                }
                onClicked: popup.selectedAction = modelData.id
                onDoubleClicked: popup.startRecording(modelData.id)
                Keys.onReturnPressed: popup.startRecording(modelData.id)
            }
        }
        Row {
            spacing: 8
            ChromeButton { objectName: "shortcutRecord"; tones: popup.tones; text: "Record…"; enabled: !popup.recordingActive; onClicked: popup.startRecording(popup.selectedAction) }
            ChromeButton { objectName: "shortcutClear"; tones: popup.tones; text: "Clear"; enabled: !popup.recordingActive; onClicked: popup.preferences.clear(popup.selectedAction) }
            ChromeButton { objectName: "shortcutReset"; tones: popup.tones; text: "Use dotfile"; enabled: !popup.recordingActive; onClicked: popup.preferences.reset(popup.selectedAction) }
        }
        Label { width: parent.width; text: popup.preferences.editError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: popup.tones.waiting; font.pixelSize: 12 }
    }
    footer: Item {
        implicitHeight: 56
        Row {
            anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter; spacing: 8
            ChromeButton { id: cancel; objectName: "shortcutCancel"; tones: popup.tones; text: "Cancel"; onClicked: popup.close() }
            ChromeButton { objectName: "shortcutApply"; tones: popup.tones; text: "Apply"; enabled: !popup.recordingActive; onClicked: if (popup.preferences.apply()) popup.close() }
        }
    }
    Popup {
        id: recorder; parent: popup.contentItem; anchors.centerIn: parent
        width: Math.min(360,parent.width); padding: 20; modal: true; focus: true
        closePolicy: Popup.NoAutoClose
        visible: popup.recordingActive
        background: Rectangle { color: popup.tones.raised; radius: 8; border.color: popup.tones.accent }
        contentItem: Column {
            focus: true
            Keys.onShortcutOverride: event => event.accepted = true
            Keys.onPressed: event => {
                event.accepted = true
                if (event.isAutoRepeat) return
                if (event.key === Qt.Key_Escape) { popup.recording = ""; return }
                if ([Qt.Key_Control,Qt.Key_Meta,Qt.Key_Shift,Qt.Key_Alt,Qt.Key_AltGr].includes(event.key)) return
                if (popup.preferences.record(popup.recording,event.key,event.modifiers)) popup.recording = ""
            }
            Keys.onReleased: event => event.accepted = true
            spacing: 12
            Label { width: parent.width; wrapMode: Text.Wrap; text: "Press the new shortcut"; color: popup.tones.foreground; font.pixelSize: 16 }
            Label { width: parent.width; wrapMode: Text.Wrap; text: "Escape cancels recording. Shell control keys and copy/paste are reserved."; color: popup.tones.muted; font.pixelSize: 12 }
            Label { width: parent.width; text: popup.preferences.editError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: popup.tones.waiting; font.pixelSize: 12 }
            ChromeButton { tones: popup.tones; text: "Cancel recording"; onClicked: popup.recording = "" }
        }

    }
    onOpened: { selectedAction = "commands"; recording = ""; cancel.forceActiveFocus() }
    onClosed: recording = ""
}
