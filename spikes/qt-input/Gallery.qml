import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design

// Every shared component in each of its states, under three terminal themes:
// the window's own, a light one and a saturated one. For design work, not for
// everyday use; opened from Commands → Design gallery. Escape closes it.
Rectangle {
    id: gallery
    property var host
    property var tones
    signal closed()
    color: tones.background
    focus: visible
    Keys.onEscapePressed: closed()

    function themeNamed(name, fallback) {
        for (const entry of studyThemes.catalog) if (entry.name === name) return studyThemes.theme(entry.index)
        return studyThemes.theme(fallback)
    }
    readonly property var themes: visible ? [
        {label: "This workspace", tones: tones},
        {label: "Workbench Dawn", tones: themeNamed("Workbench Dawn", 1)},
        {label: "Dracula", tones: themeNamed("Dracula", 2)}
    ] : []

    Row {
        id: top
        x: Design.space3xl; y: Design.spaceXl; spacing: Design.spaceL
        Text { anchors.verticalCenter: parent.verticalCenter; text: "Design gallery"; color: gallery.tones.foreground; font.pixelSize: Design.textTitle; font.weight: Font.DemiBold }
        Text { anchors.verticalCenter: parent.verticalCenter; text: "Design.qml values under three themes · Esc closes"; color: gallery.tones.muted; font.pixelSize: Design.textSmall }
    }
    ChromeButton { objectName: "galleryClose"; anchors.right: parent.right; anchors.rightMargin: Design.spaceXl; y: Design.spaceL; tones: gallery.tones; text: "Close"; onClicked: gallery.closed() }

    Flickable {
        id: flick
        x: 0; y: top.y + top.height + Design.spaceL; width: parent.width; height: parent.height - y
        contentWidth: width; contentHeight: columns.height + Design.space3xl
        clip: true; boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        // Three columns side by side when they fit, below each other when not.
        Flow {
            id: columns
            x: Design.space3xl; width: flick.width - 2 * Design.space3xl; spacing: Design.spaceXl
            // As many columns as fit at 340 pixels or wider, up to three.
            readonly property int across: Math.max(1, Math.min(3, Math.floor((width + spacing) / (340 + spacing))))
            Repeater {
                model: gallery.themes
                Rectangle {
                    id: column
                    required property var modelData
                    readonly property var t: modelData.tones
                    width: (columns.width - (columns.across - 1) * Design.spaceXl) / columns.across
                    height: body.height + 2 * Design.spaceXl
                    color: t.background; radius: Design.radiusXl; border.width: Design.line; border.color: t.border
                    Column {
                        id: body
                        x: Design.spaceXl; y: Design.spaceXl; width: parent.width - 2 * Design.spaceXl; spacing: Design.spaceXl

                        Text { text: column.modelData.label; color: column.t.muted; font.pixelSize: Design.textSmall; font.weight: Font.DemiBold }

                        // Type: each step with its role.
                        Column {
                            width: parent.width; spacing: Design.spaceXs
                            Repeater {
                                model: [["Display", Design.textDisplay], ["Title", Design.textTitle], ["Heading", Design.textHeading], ["Control", Design.textControl], ["Body", Design.textBody], ["Small", Design.textSmall], ["Caption", Design.textCaption]]
                                Text { required property var modelData; width: parent.width; elide: Text.ElideRight; text: modelData[0] + " " + modelData[1]; color: modelData[1] <= Design.textSmall ? column.t.muted : column.t.foreground; font.pixelSize: modelData[1] }
                            }
                        }

                        // Buttons in each state.
                        Flow {
                            width: parent.width; spacing: Design.spaceS
                            ChromeButton { tones: column.t; text: "Normal" }
                            ChromeButton { tones: column.t; text: "Hover"; preview: "hover" }
                            ChromeButton { tones: column.t; text: "Pressed"; preview: "down" }
                            ChromeButton { tones: column.t; text: "Focus"; preview: "focus" }
                            ChromeButton { tones: column.t; text: "Selected"; selected: true }
                            ChromeButton { tones: column.t; text: "Disabled"; enabled: false }
                            ChromeButton { tones: column.t; text: "Primary"; primary: true }
                            ChromeButton { tones: column.t; text: "Primary focus"; primary: true; preview: "focus" }
                            ChromeButton { tones: column.t; iconName: "shell"; hint: "Shell" }
                            ChromeButton { tones: column.t; iconName: "host"; hint: "Host"; preview: "hover" }
                            ChromeButton { tones: column.t; iconName: "plus"; hint: "New terminal"; selected: true }
                        }

                        // A popup surface with menu items, as menus and dialogs use it.
                        Rectangle {
                            width: parent.width; height: menu.height + 2 * Design.spaceS
                            color: column.t.raised; radius: Design.radiusXl; border.width: Design.line; border.color: column.t.border
                            Column {
                                id: menu
                                x: Design.spaceS; y: Design.spaceS; width: parent.width - 2 * Design.spaceS
                                ChromeMenuItem { width: parent.width; tones: column.t; text: "Menu item"; keyHint: studyShortcuts.labels["new-terminal"] || "Ctrl+T" }
                                ChromeMenuItem { width: parent.width; tones: column.t; text: "Hover"; preview: "hover"; keyHint: studyShortcuts.labels["apps"] || "" }
                                ChromeMenuItem { width: parent.width; tones: column.t; text: "Focus"; preview: "focus" }
                                ChromeMenuItem { width: parent.width; tones: column.t; text: "Checked"; checkable: true; checked: true }
                                ChromeMenuItem { width: parent.width; tones: column.t; text: "Disabled"; enabled: false }
                            }
                        }

                        // Key chips, marks and the theme's state colors.
                        Flow {
                            width: parent.width; spacing: Design.spaceL
                            KeyChips { tones: column.t; keys: "Ctrl+Shift+P" }
                            KeyChips { tones: column.t; keys: "⌘⇧P" }
                            Repeater {
                                model: ["accent", "ready", "waiting"]
                                Row {
                                    required property string modelData
                                    height: 18; spacing: Design.spaceXs
                                    Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: 3; color: column.t[parent.modelData] }
                                    Text { text: parent.modelData; color: column.t.muted; font.pixelSize: Design.textCaption }
                                }
                            }
                        }

                        // Radii and spacing steps.
                        Row {
                            spacing: Design.spaceM
                            Repeater {
                                model: [["S", Design.radiusS], ["M", Design.radiusM], ["L", Design.radiusL], ["XL", Design.radiusXl]]
                                Rectangle {
                                    required property var modelData
                                    width: 44; height: 32; radius: modelData[1]; color: column.t.surface || column.t.raised; border.width: Design.line; border.color: column.t.border
                                    Text { anchors.centerIn: parent; text: parent.modelData[0]; color: column.t.muted; font.pixelSize: Design.textCaption }
                                }
                            }
                        }
                        Row {
                            spacing: Design.spaceXs
                            Repeater {
                                model: [Design.space2xs, Design.spaceXs, Design.spaceS, Design.spaceM, Design.spaceL, Design.spaceXl, Design.space2xl, Design.space3xl]
                                Rectangle { required property int modelData; anchors.bottom: parent.bottom; width: modelData; height: modelData; color: column.t.accent; opacity: 0.7 }
                            }
                        }
                    }
                }
            }
        }
    }
}
