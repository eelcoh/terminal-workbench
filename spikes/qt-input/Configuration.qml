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
    width: Math.min(620,parent.width-64); height: Math.min(640,parent.height-64)
    padding: Design.space2xl; modal: true; focus: true; closePolicy: Popup.CloseOnEscape
    title: "Configuration"
    background: Rectangle { color: popup.tones.raised; radius: Design.radiusXl; border.color: popup.tones.border; SurfaceShadow {} }
    header: Label { text: popup.title; padding: Design.space2xl; bottomPadding: Design.spaceM; font.pixelSize: Design.textTitle; color: popup.tones.foreground }
    contentItem: Flickable {
        contentWidth: width; contentHeight: fields.height; clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: fields; width: parent.width; spacing: Design.spaceL
            Label {
                objectName: "configurationStatus"; visible: text.length > 0; text: popup.settings.error || popup.settings.status
                width: parent.width; wrapMode: Text.Wrap; textFormat: Text.PlainText; font.pixelSize: Design.textBody
                color: popup.settings.error.length ? popup.tones.waiting : popup.tones.foreground
            }
            // Settings in cards (owner, 2026-11-12, after Rex).
            Item {
                width: fields.width; height: Math.max(windowHeading.implicitHeight, viewDefaults.visible ? viewDefaults.height : 0)
                Label { id: windowHeading; anchors.verticalCenter: parent.verticalCenter; leftPadding: Design.spaceXs; text: "This window"; font.pixelSize: Design.textBody; font.weight: Font.DemiBold; color: popup.tones.foreground }
                // Only when this window chose differently from config.conf.
                ChromeButton {
                    id: viewDefaults; objectName: "configurationViewDefaults"; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    tones: popup.tones; text: "Use config.conf"; visible: !popup.navigationInherited || !popup.motionInherited; onClicked: popup.viewDefaultsRequested()
                }
            }
            SettingsCard {
                tones: popup.tones
                SettingsRow { tones: popup.tones; title: "Sidebar"; value: (popup.navigationPinned ? "Pinned" : "Drawer") + (popup.navigationInherited ? "" : " · this window's choice") }
                SettingsRow { tones: popup.tones; title: "Motion"; value: (popup.reducedMotion ? "Reduced" : "Animated") + (popup.motionInherited ? "" : " · this window's choice") }
                SettingsRow { tones: popup.tones; title: "Unfocused panes"; value: popup.settings.dimUnfocused ? "Dimmed" : "Not dimmed" }
                SettingsRow { tones: popup.tones; visible: Design.apple; title: "Top bar and sidebar"; value: popup.settings.windowMaterial ? "System material" : "Opaque" }
            }
            SettingsCard {
                tones: popup.tones; title: "New panes"
                SettingsRow { tones: popup.tones; title: "Workspace folder"; value: popup.settings.workspaceDirectory || "The current workspace's folder" }
                SettingsRow { tones: popup.tones; title: "Terminal width"; value: popup.settings.terminalWidth === 1 ? "Full width" : popup.settings.terminalWidth < 0.4 ? "⅓" : popup.settings.terminalWidth > 0.6 ? "⅔" : "½" }
            }
            SettingsCard {
                tones: popup.tones; title: "Files · config.conf, then this platform's overrides"
                Repeater {
                    model: [{title: "Settings",path: popup.settings.managedPath},
                        {title: "Font",path: popup.settings.fontPath},
                        {title: "Shortcuts",path: popup.settings.shortcutPath},
                        {title: "Workspace colors",path: popup.workspaceAppearancePath}]
                    SettingsRow { required property var modelData; tones: popup.tones; title: modelData.title; value: modelData.path || "Not configured"; selectable: true }
                }
            }
            Label { leftPadding: Design.spaceXs; text: "Band follows changes to these files. Workspaces with their own theme keep it; open editors keep their drafts."; width: parent.width; wrapMode: Text.Wrap; font.pixelSize: Design.textSmall; color: popup.tones.muted }
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
