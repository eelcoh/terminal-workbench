import QtQuick
import QtQuick.Controls
Dialog {
    id: popup
    required property var tones
    required property var settings
    property string workspaceAppearancePath: ""
    width: Math.min(620,parent.width-64); height: Math.min(480,parent.height-64)
    padding: 20; modal: true; focus: true; closePolicy: Popup.CloseOnEscape
    title: "Configuration"
    background: Rectangle { color: popup.tones.raised; radius: 12; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: 20; bottomPadding: 8; font.pixelSize: 22; color: popup.tones.foreground }
    contentItem: Flickable {
        contentWidth: width; contentHeight: fields.height; clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: fields; width: parent.width; spacing: 10
            Label { text: "Shared → platform → UI overrides"; font.pixelSize: 13; color: popup.tones.muted }
            Repeater {
                model: [{title: "Managed settings",path: popup.settings.managedPath},
                    {title: "Font UI overrides",path: popup.settings.fontPath},
                    {title: "Shortcut UI overrides",path: popup.settings.shortcutPath},
                    {title: "Workspace palettes · shared by session windows",path: popup.workspaceAppearancePath}]
                Column {
                    required property var modelData
                    width: fields.width; spacing: 3
                    Label { text: modelData.title; font.pixelSize: 13; color: popup.tones.foreground }
                    TextEdit { width: parent.width; height: contentHeight; text: modelData.path || "Not configured"; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; textFormat: TextEdit.PlainText; color: popup.tones.muted; font.pixelSize: 12 }
                }
            }
            Label { text: "Fonts, shortcuts and the workspace default theme follow file changes. Workspaces with a chosen theme keep their colors. Open editors keep their drafts."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: 13; color: popup.tones.muted }
            Label { text: "New workspace folder: " + (popup.settings.workspaceDirectory || "Current workspace folder"); width: parent.width; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText; font.pixelSize: 13; color: popup.tones.muted }
            Label { text: "New terminal width: " + (popup.settings.terminalWidth === 1 ? "Full width" : popup.settings.terminalWidth < 0.4 ? "⅓" : popup.settings.terminalWidth > 0.6 ? "⅔" : "½"); font.pixelSize: 13; color: popup.tones.muted }
            Label { objectName: "configurationStatus"; text: popup.settings.error || popup.settings.status; width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pixelSize: 13; color: popup.settings.error.length ? popup.tones.waiting : popup.tones.foreground }
        }
    }
    footer: Item {
        implicitHeight: 52
        Row {
            x: 20; spacing: 8
            ChromeButton { tones: popup.tones; text: "Open folder"; enabled: popup.settings.managedPath.length > 0; onClicked: popup.settings.openDirectory() }
            ChromeButton { objectName: "configurationReload"; tones: popup.tones; text: "Reload"; enabled: !popup.settings.editing; onClicked: popup.settings.reload() }
        }
        ChromeButton { id: done; objectName: "configurationClose"; anchors.right: parent.right; anchors.rightMargin: 20; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    onOpened: done.forceActiveFocus()
}
