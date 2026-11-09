import QtQuick
import QtQuick.Controls
import Band.Design
Dialog {
    id: popup
    required property var tones
    required property var settings
    property bool navigationInherited: true
    property bool motionInherited: true
    property bool navigationPinned: false
    property bool reducedMotion: false
    signal viewDefaultsRequested()
    property string workspaceAppearancePath: ""
    width: Math.min(620,parent.width-64); height: Math.min(540,parent.height-64)
    padding: Design.space2xl; modal: true; focus: true; closePolicy: Popup.CloseOnEscape
    title: "Configuration"
    background: Rectangle { color: popup.tones.raised; radius: Design.radiusXl; border.color: popup.tones.border }
    header: Label { text: popup.title; padding: Design.space2xl; bottomPadding: Design.spaceM; font.pixelSize: Design.textTitle; color: popup.tones.foreground }
    contentItem: Flickable {
        contentWidth: width; contentHeight: fields.height; clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: fields; width: parent.width; spacing: Design.spaceM
            Label {
                objectName: "configurationStatus"; visible: text.length > 0; text: popup.settings.error || popup.settings.status
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pixelSize: Design.textBody
                color: popup.settings.error.length ? popup.tones.waiting : popup.tones.foreground
            }
            Item {
                width: fields.width; height: Math.max(windowHeading.height, viewDefaults.visible ? viewDefaults.height : 0)
                Label { id: windowHeading; anchors.verticalCenter: parent.verticalCenter; text: "This window"; width: fields.width; topPadding: Design.spaceS; font.pixelSize: Design.textSmall; font.weight: Font.Bold; color: popup.tones.muted }
                // Only when this window chose differently from config.conf.
                ChromeButton {
                    id: viewDefaults; objectName: "configurationViewDefaults"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    tones: popup.tones; text: "Use config.conf"; visible: !popup.navigationInherited || !popup.motionInherited; onClicked: popup.viewDefaultsRequested()
                }
            }
            Item {
                width: fields.width; height: Math.max(implicitHeight, childrenRect.height)
                Label { width: parent.width * 0.4; text: "Sidebar"; font.pixelSize: Design.textBody; color: popup.tones.foreground }
                Label { x: parent.width * 0.4; width: parent.width * 0.6; text: (popup.navigationPinned ? "Pinned" : "Drawer") + (popup.navigationInherited ? "" : " · this window's choice"); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Design.textBody; color: popup.tones.muted }
            }
            Item {
                width: fields.width; height: Math.max(implicitHeight, childrenRect.height)
                Label { width: parent.width * 0.4; text: "Motion"; font.pixelSize: Design.textBody; color: popup.tones.foreground }
                Label { x: parent.width * 0.4; width: parent.width * 0.6; text: (popup.reducedMotion ? "Reduced" : "Animated") + (popup.motionInherited ? "" : " · this window's choice"); textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Design.textBody; color: popup.tones.muted }
            }
            Label { text: "New panes"; width: fields.width; topPadding: Design.spaceS; font.pixelSize: Design.textSmall; font.weight: Font.Bold; color: popup.tones.muted }
            Item {
                width: fields.width; height: Math.max(implicitHeight, childrenRect.height)
                Label { width: parent.width * 0.4; text: "Workspace folder"; font.pixelSize: Design.textBody; color: popup.tones.foreground }
                Label { x: parent.width * 0.4; width: parent.width * 0.6; text: popup.settings.workspaceDirectory || "The current workspace's folder"; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Design.textBody; color: popup.tones.muted }
            }
            Item {
                width: fields.width; height: Math.max(implicitHeight, childrenRect.height)
                Label { width: parent.width * 0.4; text: "Terminal width"; font.pixelSize: Design.textBody; color: popup.tones.foreground }
                Label { x: parent.width * 0.4; width: parent.width * 0.6; text: popup.settings.terminalWidth === 1 ? "Full width" : popup.settings.terminalWidth < 0.4 ? "⅓" : popup.settings.terminalWidth > 0.6 ? "⅔" : "½"; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; font.pixelSize: Design.textBody; color: popup.tones.muted }
            }
            Label { text: "Files · config.conf, then this platform's overrides"; width: fields.width; topPadding: Design.spaceS; font.pixelSize: Design.textSmall; font.weight: Font.Bold; color: popup.tones.muted }
            Repeater {
                model: [{title: "Settings",path: popup.settings.managedPath},
                    {title: "Font",path: popup.settings.fontPath},
                    {title: "Shortcuts",path: popup.settings.shortcutPath},
                    {title: "Workspace colors",path: popup.workspaceAppearancePath}]
                Item {
                    required property var modelData
                    width: fields.width; height: pathText.contentHeight
                    Label { width: parent.width * 0.4; text: modelData.title; font.pixelSize: Design.textBody; color: popup.tones.foreground }
                    TextEdit { id: pathText; x: parent.width * 0.4; width: parent.width * 0.6; text: modelData.path || "Not configured"; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; textFormat: TextEdit.PlainText; color: popup.tones.muted; font.pixelSize: Design.textSmall }
                }
            }
            Label { topPadding: Design.spaceS; text: "Band follows changes to these files. Workspaces with their own theme keep it; open editors keep their drafts."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textSmall; color: popup.tones.muted }
        }
    }
    footer: Item {
        implicitHeight: 56
        Row {
            x: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; spacing: Design.spaceXs
            ChromeButton { tones: popup.tones; text: "Open folder"; enabled: popup.settings.managedPath.length > 0; onClicked: popup.settings.openDirectory() }
            ChromeButton { objectName: "configurationReload"; tones: popup.tones; text: "Reload"; enabled: !popup.settings.editing; onClicked: popup.settings.reload() }
        }
        ChromeButton { id: done; objectName: "configurationClose"; anchors.right: parent.right; anchors.rightMargin: Design.space2xl; anchors.verticalCenter: parent.verticalCenter; tones: popup.tones; text: "Close"; onClicked: popup.close() }
    }
    onOpened: done.forceActiveFocus()
}
