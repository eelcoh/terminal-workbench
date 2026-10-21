import QtQuick
import QtQuick.Controls
Dialog {
    id: popup
    required property var tones
    required property var model
    required property var facts
    property string contextLabel: ""
    readonly property var information: model.describe(facts)
    property bool copied: false
    onInformationChanged: copied = false
    width: Math.min(560,parent.width-64); height: Math.min(520,parent.height-64)
    padding: 20; modal: true; focus: true; closePolicy: Popup.CloseOnEscape
    title: "Agent details"
    background: Rectangle { color: popup.tones.raised; radius: 12; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: 20; bottomPadding: 8; font.pixelSize: 22; color: popup.tones.foreground }
    contentItem: Flickable {
        contentWidth: width; contentHeight: fields.height; clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: fields; width: parent.width; spacing: 14
            Label { text: popup.contextLabel; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: 12; color: popup.tones.muted }
            Label { text: popup.information.agent; visible: text.length > 0; font.pixelSize: 20; color: popup.tones.foreground }
            Label { text: popup.information.heading; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 15; color: popup.tones.foreground }
            Label { text: popup.information.meaning; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.foreground }
            Label { text: "States are the last runtime reports. Missing completion events after cancellation can leave a stale state until the next report or foreground exit."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.muted }
            Column {
                visible: popup.information.launcher.length > 0; width: parent.width; spacing: 8
                Label { text: "Optional lifecycle launcher"; font.pixelSize: 14; color: popup.tones.foreground }
                TextEdit { width: parent.width; text: popup.information.launcher; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; color: popup.tones.foreground; font.pixelSize: 14 }
                ChromeButton { objectName: "agentInfoCopy"; tones: popup.tones; text: popup.copied ? "Copied" : "Copy command"; onClicked: popup.copied = popup.model.copyLauncher(popup.information.agent) }
                Label { text: "Run this yourself in a new Workbench terminal when starting an agent. It adds invocation-local observers. Existing settings, trust and permission policies still apply. Older terminals retain their original helpers."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.muted }
            }
        }
    }
    footer: Item {
        implicitHeight: 52
        ChromeButton { id: done; objectName: "agentInfoClose"; anchors.right: parent.right; anchors.rightMargin: 20; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    onOpened: { copied = false; done.forceActiveFocus() }
}
