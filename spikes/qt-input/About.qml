import QtQuick
import QtQuick.Controls
import Band.Design
Dialog {
    id: popup
    required property var tones
    required property var build
    property bool expanded: false
    property bool copied: false
    readonly property bool detailsReady: expanded && report.visible && report.width > 0 && report.height > 0 && details.height > 0
    width: Math.min(540, parent.width - 64)
    height: Math.min(expanded ? 450 : 260, parent.height - 64)
    padding: Design.space2xl; modal: true; focus: true
    closePolicy: Popup.CloseOnEscape
    title: "Band"
    background: Rectangle { color: popup.tones.raised; radius: Design.radiusXl; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: Design.space2xl; bottomPadding: Design.spaceM; color: popup.tones.foreground; font.pixelSize: Design.textTitle }
    contentItem: Item {
        Label { id: revision; width: parent.width; text: popup.build.summary; textFormat: Text.PlainText; color: popup.tones.muted; font.pixelSize: Design.textBody; elide: Text.ElideMiddle }
        Row {
            id: actions; anchors.top: revision.bottom; anchors.topMargin: Design.spaceXl; spacing: Design.spaceM
            ChromeButton { objectName: "aboutCopy"; tones: popup.tones; text: popup.copied ? "Copied" : "Copy diagnostics"; onClicked: { popup.build.copyDiagnostics(); popup.copied = true } }
            ChromeButton { objectName: "aboutDetails"; tones: popup.tones; text: popup.expanded ? "Hide details" : "Details"; onClicked: popup.expanded = !popup.expanded }
        }
        Flickable {
            id: details
            contentWidth: width; contentHeight: report.height
            ScrollBar.vertical: ScrollBar {}
            anchors.top: actions.bottom; anchors.topMargin: Design.spaceL; anchors.bottom: privacy.top; anchors.bottomMargin: Design.spaceL
            width: parent.width; visible: popup.expanded; clip: true
            TextEdit {
                id: report; objectName: "aboutReport"; width: parent.width; height: contentHeight; text: popup.build.diagnostics; readOnly: true; selectByMouse: true
                textFormat: TextEdit.PlainText; wrapMode: TextEdit.WrapAnywhere
                color: popup.tones.foreground; font.pixelSize: Design.textSmall
            }
        }
        Label {
            id: privacy; anchors.bottom: parent.bottom; width: parent.width
            text: "Build and installation details only. Includes local paths."
            wrapMode: Text.Wrap; color: popup.tones.muted; font.pixelSize: Design.textSmall
        }
    }
    footer: Item {
        implicitHeight: 52
        ChromeButton { id: closeButton; objectName: "aboutClose"; anchors.right: parent.right; anchors.rightMargin: Design.space2xl; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    onOpened: { copied = false; expanded = false; closeButton.forceActiveFocus() }
}
