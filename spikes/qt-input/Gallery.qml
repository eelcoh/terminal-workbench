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
    // End jumps to Band in place, Home back to the top.
    Keys.onPressed: event => {
        if (event.key === Qt.Key_End) { flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, scenes.y - Design.spaceL)); event.accepted = true }
        else if (event.key === Qt.Key_Home) { flick.contentY = 0; event.accepted = true }
    }

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
        Text { id: galleryTitle; anchors.verticalCenter: parent.verticalCenter; text: "Design gallery"; color: gallery.tones.foreground; font.pixelSize: Design.textTitle; font.weight: Font.DemiBold }
        Text { anchors.verticalCenter: parent.verticalCenter; width: Math.max(0, Math.min(implicitWidth, gallery.width - top.x - galleryTitle.width - top.spacing - galleryClose.width - 2 * Design.spaceXl)); elide: Text.ElideRight; text: "Design.qml values under three themes · End shows Band in place · Esc closes"; color: gallery.tones.muted; font.pixelSize: Design.textSmall }
    }
    ChromeButton { id: galleryClose; objectName: "galleryClose"; anchors.right: parent.right; anchors.rightMargin: Design.spaceXl; y: Design.spaceL; tones: gallery.tones; text: "Close"; onClicked: gallery.closed() }

    Flickable {
        id: flick
        x: 0; y: top.y + top.height + Design.spaceL; width: parent.width; height: parent.height - y
        contentWidth: width; contentHeight: scenes.y + scenes.height + Design.space3xl
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
        // The window's own parts together: top bar, sidebar rows and pane
        // headers, with sample workspaces and terminals.
        Column {
            id: scenes
            x: Design.space3xl; y: columns.y + columns.height + Design.space3xl; width: flick.width - 2 * Design.space3xl; spacing: Design.spaceXl
            Repeater {
                model: gallery.themes
                Rectangle {
                    id: scene
                    required property var modelData
                    readonly property var t: modelData.tones
                    width: scenes.width; height: 300
                    color: t.background; radius: Design.radiusXl; border.width: Design.line; border.color: t.border; clip: true
                    Rectangle { x: Design.line; y: Design.line; width: parent.width - 2 * Design.line; height: 40; radius: Design.radiusXl; color: scene.t.surface }
                    Rectangle { x: Design.line; y: 40; width: 240; height: parent.height - 40 - Design.line; color: scene.t.surface }
                    Text { x: Design.spaceXl; y: 12; text: scene.modelData.label; color: scene.t.muted; font.pixelSize: Design.textSmall; font.weight: Font.DemiBold }
                    TopBar {
                        x: 248; y: 4; width: parent.width - x - Design.spaceL
                        tones: scene.t; workspaceName: "Band"; focusPosition: 1
                        markers: [{title: "claude", selected: false, waiting: true}, {title: "zsh", selected: true, waiting: false}, {title: "htop", selected: false, waiting: false}]
                        inView: position => position < 2
                    }
                    Column {
                        x: Design.spaceM; y: 48; width: 240 - 2 * Design.spaceM; spacing: Design.space2xs
                        Item {
                            width: parent.width; height: 36
                            SidebarWorkspace { width: parent.width - 28; tones: scene.t; text: "Band"; count: 3; waiting: 1; dirty: true; selected: true }
                            ChromeButton { x: parent.width - 26; anchors.verticalCenter: parent.verticalCenter; width: 24; height: 28; tones: scene.t; iconName: "chevron"; hint: "Hide panes" }
                        }
                        SidebarPane { x: Design.spaceL; width: parent.width - Design.spaceL; tones: scene.t; kindIcon: "agents"; iconTone: "waiting"; title: "claude"; detail: "~/Code/band · main" }
                        SidebarPane { x: Design.spaceL; width: parent.width - Design.spaceL; tones: scene.t; title: "zsh"; detail: "~/Code/band/crates · main"; dirty: true; selected: true }
                        SidebarPane { x: Design.spaceL; width: parent.width - Design.spaceL; tones: scene.t; kindIcon: "apps"; iconTone: "waiting"; title: "htop"; detail: "Stopped" }
                        Item {
                            width: parent.width; height: 36
                            SidebarWorkspace { width: parent.width - 28; tones: scene.t; text: "Side project"; branch: "feature/sync"; count: 2 }
                            ChromeButton { x: parent.width - 26; anchors.verticalCenter: parent.verticalCenter; width: 24; height: 28; rotation: -90; tones: scene.t; iconName: "chevron"; hint: "Show panes" }
                        }
                    }
                    // Side by side when each pane gets 320 px, stacked otherwise.
                    Grid {
                        x: 252; y: 48; spacing: Design.spaceL
                        readonly property bool wide: scene.width - x - Design.spaceL - spacing >= 640
                        columns: wide ? 2 : 1
                        readonly property real paneWidth: wide ? (scene.width - x - Design.spaceL - spacing) / 2 : scene.width - x - Design.spaceL
                        readonly property real paneHeight: wide ? scene.height - 60 : (scene.height - 60 - spacing) / 2
                        Repeater {
                            model: [{name: "claude", place: "~/Code/band", icon: "agents", agentState: "Needs input", waiting: true, focused: false, dirty: false, own: -1},
                                    {name: "zsh", place: "~/Code/band/crates", icon: "shell", agentState: "", waiting: false, focused: true, dirty: true, own: 2}]
                            Rectangle {
                                required property var modelData
                                width: parent.paneWidth; height: parent.paneHeight
                                color: scene.t.surface; radius: Design.radiusM; border.width: Design.line; border.color: modelData.focused ? scene.t.accent : scene.t.border
                                PaneHeader {
                                    width: parent.width; tones: scene.t; icon: parent.modelData.icon; name: parent.modelData.name; place: parent.modelData.place
                                    agentState: parent.modelData.agentState; needsInput: parent.modelData.waiting; focused: parent.modelData.focused; dirty: parent.modelData.dirty
                                    ownTheme: parent.modelData.own; colorsEnabled: false
                                }
                                Text { x: 12; y: 44; text: "$ "; color: scene.t.foreground; font.family: "monospace"; font.pixelSize: Design.textBody }
                            }
                        }
                    }
                }
            }
        }
    }
}
