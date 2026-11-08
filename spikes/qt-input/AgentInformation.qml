import QtQuick
import QtQuick.Controls
import Band.Design
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
    padding: Design.space2xl; modal: true; focus: true; closePolicy: Popup.CloseOnEscape
    title: "Agent details"
    background: Rectangle { color: popup.tones.raised; radius: Design.radiusXl; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: Design.space2xl; bottomPadding: Design.spaceM; font.pixelSize: Design.textTitle; color: popup.tones.foreground }
    contentItem: Flickable {
        contentWidth: width; contentHeight: fields.height; clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: fields; width: parent.width; spacing: Design.spaceXl
            Label { text: popup.contextLabel; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Design.textSmall; color: popup.tones.muted }
            Label { text: popup.information.agent; visible: text.length > 0; font.pixelSize: Design.textTitle; color: popup.tones.foreground }
            Label { text: popup.information.heading; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textHeading; color: popup.tones.foreground }
            Label { text: popup.information.meaning; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textBody; color: popup.tones.foreground }
            Label { text: "States are the last runtime reports. Missing completion events after cancellation can leave a stale state until the next report or foreground exit."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textBody; color: popup.tones.muted }
            Column {
                visible: popup.information.launcher.length > 0; width: parent.width; spacing: Design.spaceM
                Label { text: "Optional lifecycle launcher"; font.pixelSize: Design.textControl; color: popup.tones.foreground }
                TextEdit { width: parent.width; text: popup.information.launcher; textFormat: TextEdit.PlainText; readOnly: true; selectByMouse: true; color: popup.tones.foreground; font.pixelSize: Design.textControl }
                ChromeButton { objectName: "agentInfoCopy"; tones: popup.tones; text: popup.copied ? "Copied" : "Copy command"; onClicked: popup.copied = popup.model.copyLauncher(popup.information.agent) }
                Label { text: "Run this yourself in a new Workbench terminal when starting an agent. It adds invocation-local observers. Existing settings, trust and permission policies still apply. Older terminals retain their original helpers."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textBody; color: popup.tones.muted }
            }
        }
    }
    signal terminalDetailsRequested()
    footer: Item {
        implicitHeight: 52
        ChromeButton { objectName: "agentInfoRuntime"; x: 20; tones: popup.tones; text: "Terminal details…"; onClicked: popup.terminalDetailsRequested() }
        ChromeButton { id: done; objectName: "agentInfoClose"; anchors.right: parent.right; anchors.rightMargin: Design.space2xl; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    onOpened: { copied = false; done.forceActiveFocus() }
}
