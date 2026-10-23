import QtQuick
import QtQuick.Controls
Dialog {
    id: popup
    required property var tones
    required property var build
    property bool expanded: false
    property bool copied: false
    readonly property bool detailsReady: expanded && report.visible && report.width > 0 && report.height > 0 && details.height > 0
    width: Math.min(540, parent.width - 64)
    height: Math.min(expanded ? 450 : 260, parent.height - 64)
    padding: 20; modal: true; focus: true
    closePolicy: Popup.CloseOnEscape
    title: "Band"
    background: Rectangle { color: popup.tones.raised; radius: 12; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: 20; bottomPadding: 8; color: popup.tones.foreground; font.pixelSize: 22 }
    contentItem: Item {
        Label { id: revision; width: parent.width; text: popup.build.summary; textFormat: Text.PlainText; color: popup.tones.muted; font.pixelSize: 13; elide: Text.ElideMiddle }
        Row {
            id: actions; anchors.top: revision.bottom; anchors.topMargin: 16; spacing: 8
            ChromeButton { objectName: "aboutCopy"; tones: popup.tones; text: popup.copied ? "Copied" : "Copy diagnostics"; onClicked: { popup.build.copyDiagnostics(); popup.copied = true } }
            ChromeButton { objectName: "aboutDetails"; tones: popup.tones; text: popup.expanded ? "Hide details" : "Details"; onClicked: popup.expanded = !popup.expanded }
        }
        Flickable {
            id: details
            contentWidth: width; contentHeight: report.height
            ScrollBar.vertical: ScrollBar {}
            anchors.top: actions.bottom; anchors.topMargin: 12; anchors.bottom: privacy.top; anchors.bottomMargin: 12
            width: parent.width; visible: popup.expanded; clip: true
            TextEdit {
                id: report; objectName: "aboutReport"; width: parent.width; height: contentHeight; text: popup.build.diagnostics; readOnly: true; selectByMouse: true
                textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere
                color: popup.tones.foreground; font.pixelSize: 12
            }
        }
        Label {
            id: privacy; anchors.bottom: parent.bottom; width: parent.width
            text: "Build and installation details only. Includes local paths."
            wrapMode: Text.Wrap; color: popup.tones.muted; font.pixelSize: 12
        }
    }
    footer: Item {
        implicitHeight: 52
        ChromeButton { id: closeButton; objectName: "aboutClose"; anchors.right: parent.right; anchors.rightMargin: 20; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    onOpened: { copied = false; expanded = false; closeButton.forceActiveFocus() }
}
