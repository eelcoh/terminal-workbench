import QtQuick
import QtQuick.Controls
import Workbench.Probe
import Band.Design
// Appearance and Terminal colors: search, All/Dark/Light, theme cards with a
// live preview, and the default/Cancel/Apply row. It follows the window's
// chrome tones like every other dialog.
Dialog {
    id: picker
    objectName: "appearanceDialog"
    required property var tones
    property var catalog: []
    property int themeIndex: 0
    property string saveError: ""
    property bool followsDefault: false
    property string defaultName: ""
    signal defaultRequested()
    // Makes the selected theme Band's default for new workspaces.
    property bool canMakeDefault: false
    signal makeDefaultRequested()
    property string workspaceName: ""
    // Workspace colors by default; the terminal mode changes only these texts.
    property string heading: "Appearance"
    property string scope: "all terminals in this workspace"
    property string useDefaultText: "Use default"
    property string usingDefaultText: "Using default"
    property string note: ""
    property int mode: 0
    property bool preparing: false
    property var retainedTheme: ({})
    property string directoryPath: ""
    property string directoryStatus: ""
    signal folderRequested()
    readonly property bool helpOpen: themeHelp.visible
    readonly property bool searchFocused: search.activeFocus
    signal previewSelected(int index)
    signal applySelected()
    readonly property var matches: catalog.filter(theme => theme.name.toLowerCase().includes(search.text.toLowerCase()) && (mode === 0 || theme.light === (mode === 2)))
    title: heading + " · " + workspaceName
    implicitWidth: 560; implicitHeight: 600
    modal: true; width: Math.min(implicitWidth, parent.width - 64); height: Math.min(implicitHeight, parent.height - 64)
    padding: Design.space2xl; closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "transparent" }
    background: Rectangle { color: picker.tones.raised; radius: Design.radiusXl; border.width: Design.line; border.color: picker.tones.border; SurfaceShadow {} }
    header: Label { width: picker.width; text: picker.title; textFormat: Text.PlainText; padding: Design.space2xl; bottomPadding: Design.spaceM; color: picker.tones.foreground; font.pixelSize: Design.textTitle - 2; elide: Text.ElideRight }
    onClosed: themeHelp.close()
    onAboutToShow: preparing = true
    onOpened: { search.text = ""; mode = 0; list.currentIndex = -1; search.forceActiveFocus(); preparing = false }
    function revealTheme(index) {
        search.text = ""; mode = 0
        list.currentIndex = matches.findIndex(theme => theme.index === index)
        if (list.currentIndex >= 0) list.positionViewAtIndex(list.currentIndex,GridView.Contain)
        list.forceActiveFocus()
    }
    component ActionButton: ChromeButton { tones: picker.tones }
    contentItem: Column {
        spacing: Design.spaceL
        TextField {
            id: search; objectName: "themeSearch"; width: parent.width; placeholderText: "Search themes"; selectByMouse: true
            color: picker.tones.foreground; selectionColor: picker.tones.accent; selectedTextColor: picker.tones.background; placeholderTextColor: picker.tones.muted; padding: Design.spaceL
            font.pixelSize: Design.textBody
            background: Rectangle { color: picker.tones.background; radius: Design.radiusM; border.width: Design.line; border.color: search.activeFocus ? picker.tones.accent : picker.tones.border }
            Keys.onDownPressed: if (list.count) { list.currentIndex = 0; list.forceActiveFocus(); picker.previewSelected(picker.matches[0].index) }
        }
        Item {
            id: modes; width: parent.width; height: 32
            Row {
                spacing: Design.spaceXs
                Repeater {
                    model: ["All", "Dark", "Light"]
                    ActionButton { required property int index; required property string modelData; text: modelData; selected: picker.mode === index; onClicked: picker.mode = index }
                }
            }
            ActionButton {
                id: helpButton; objectName: "themeHelpButton"
                visible: picker.directoryPath.length > 0
                anchors.right: parent.right; iconName: "help"; hint: "Help with themes"
                Accessible.name: "Help with themes"
                onClicked: themeHelp.open()
            }
        }
        Label { id: folderStatus; width: parent.width; visible: picker.directoryStatus.length > 0; text: picker.directoryStatus; textFormat: Text.PlainText; elide: Text.ElideRight; color: picker.tones.muted; font.pixelSize: Design.textCaption }
        // Themes as cards, as in Welcome: the theme's own background, its name in its
        // foreground, six swatches; the previewed theme carries the accent and a check.
        GridView {
            id: list; objectName: "themeList"; width: parent.width; height: Math.max(80, parent.height - search.height - modes.height - previewLabel.implicitHeight - sourceLabel.implicitHeight - (folderStatus.visible ? folderStatus.implicitHeight + parent.spacing : 0) - (saveErrorLabel.visible ? saveErrorLabel.implicitHeight + parent.spacing : 0) - (noteLabel.visible ? noteLabel.implicitHeight + parent.spacing : 0) - parent.spacing * 4)
            clip: true; model: picker.matches; currentIndex: -1; keyNavigationWraps: false
            readonly property int columns: Math.max(1, Math.floor(width / 150))
            cellWidth: Math.floor(width / columns); cellHeight: 84
            ScrollBar.vertical: ScrollBar {}
            onCurrentIndexChanged: if (picker.visible && !picker.preparing && activeFocus && currentIndex >= 0 && currentIndex < count) picker.previewSelected(picker.matches[currentIndex].index)
            Keys.onReturnPressed: picker.applySelected()
            Keys.onUpPressed: event => { if (currentIndex < columns) search.forceActiveFocus(); else event.accepted = false }
            delegate: Item {
                id: option; required property var modelData; required property int index
                readonly property bool previewed: picker.themeIndex === modelData.index
                readonly property bool keyed: list.activeFocus && list.currentIndex === index
                width: list.cellWidth; height: list.cellHeight
                Accessible.role: Accessible.Button; Accessible.name: modelData.name + (modelData.light ? ", light" : ", dark")
                Rectangle {
                    id: card; x: Design.space2xs; y: Design.space2xs; width: parent.width - 2 * Design.space2xs - Design.spaceXs; height: parent.height - Design.spaceS
                    radius: Design.radiusL; color: option.modelData.background
                    border.width: option.previewed || option.keyed ? Design.lineStrong : Design.line
                    border.color: option.previewed || option.keyed ? picker.tones.accent : cardHover.hovered ? picker.tones.muted : picker.tones.border
                    HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
                    Text { x: 10; y: 10; width: parent.width - 34; elide: Text.ElideRight; text: option.modelData.name; textFormat: Text.PlainText; color: option.modelData.foreground; font.pixelSize: Design.textSmall; font.weight: Font.DemiBold }
                    ChromeIcon { anchors.right: parent.right; anchors.rightMargin: Design.spaceM; y: 8; width: 16; height: 16; name: "check"; color: option.modelData.accent; visible: option.previewed }
                    Row {
                        x: 10; y: parent.height - 24; spacing: Design.spaceXs
                        Repeater { model: option.modelData.swatches.slice(1, 7); Rectangle { required property string modelData; width: 14; height: 14; radius: Design.radiusS; color: modelData } }
                    }
                    TapHandler { onTapped: { list.currentIndex = option.index; picker.previewSelected(option.modelData.index) } }
                }
            }
            Label { anchors.centerIn: parent; visible: list.count === 0; text: "No matching themes"; color: picker.tones.muted }
        }
        Label { id: saveErrorLabel; objectName: "workspaceAppearanceError"; width: parent.width; visible: picker.saveError.length > 0; text: picker.saveError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: picker.tones.waiting; font.pixelSize: Design.textSmall }
        Label { id: previewLabel; width: parent.width; textFormat: Text.PlainText; text: "Previewing " + ((picker.catalog.find(theme => theme.index === picker.themeIndex) || picker.retainedTheme)?.name || "Current workspace theme") + " · " + picker.scope; color: picker.tones.muted; font.pixelSize: Design.textSmall; elide: Text.ElideRight }
        Label { id: noteLabel; objectName: "appearanceNote"; width: parent.width; visible: picker.note.length > 0; text: picker.note; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: picker.tones.muted; font.pixelSize: Design.textCaption }
        Label {
            id: sourceLabel; width: parent.width; color: picker.tones.muted; font.pixelSize: Design.textCaption; textFormat: Text.PlainText; elide: Text.ElideMiddle
            readonly property var theme: picker.catalog.find(value => value.index === picker.themeIndex) || picker.retainedTheme
            text: theme?.imported ? "Source: " + theme.source + (theme.attribution ? " · " + theme.attribution : "") : "Bundled offline palette"
        }
    }
    Popup {
        id: themeHelp; objectName: "themeHelpPopup"
        parent: picker.contentItem; anchors.centerIn: parent
        width: Math.min(380, parent.width); padding: Design.spaceXl
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: "transparent" }
        background: Rectangle { color: picker.tones.raised; radius: Design.radiusL; border.width: Design.line; border.color: picker.tones.border; SurfaceShadow {} }
        contentItem: Column {
            spacing: Design.spaceL
            Label { text: "Add your own themes"; color: picker.tones.foreground; font.pixelSize: Design.textControl }
            Label {
                width: parent.width; wrapMode: Text.WordWrap; color: picker.tones.foreground; font.pixelSize: Design.textSmall
                text: "Drop Ghostty theme files in this folder, including TerminalColors .default downloads. Reopen Appearance to see them in the list, then select a theme and Apply. No import or file opening is needed."
            }
            Label { width: parent.width; text: picker.directoryPath; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: picker.tones.muted; font.pixelSize: Design.textCaption }
            Label {
                objectName: "themesFolderLink"
                text: '<a href="themes-folder" style="color: ' + picker.tones.accent + '">Open themes folder</a>'
                textFormat: Text.RichText; font.pixelSize: Design.textSmall
                Accessible.role: Accessible.Link; Accessible.name: "Open themes folder in file manager"
                activeFocusOnTab: true
                onLinkActivated: picker.folderRequested()
                Keys.onReturnPressed: picker.folderRequested()
                Keys.onSpacePressed: picker.folderRequested()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                Rectangle { anchors.fill: parent; anchors.margins: -2; visible: parent.activeFocus; color: "transparent"; radius: Design.radiusS; border.color: picker.tones.accent }
            }
            ActionButton { text: "Got it"; onClicked: themeHelp.close() }
        }
    }
    // One row: the default on the left, the decision on the right.
    footer: Item {
        implicitHeight: 64
        ActionButton {
            id: defaultButton; objectName: "workspaceThemeDefault"; x: Design.space2xl; anchors.verticalCenter: decision.verticalCenter
            text: picker.followsDefault ? picker.usingDefaultText : picker.useDefaultText; enabled: !picker.followsDefault
            onClicked: picker.defaultRequested()
        }
        Label {
            anchors.left: defaultButton.right; anchors.leftMargin: Design.spaceS; anchors.right: decision.left; anchors.rightMargin: Design.spaceM; anchors.verticalCenter: decision.verticalCenter
            text: picker.defaultName; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: Design.textSmall; color: picker.tones.muted
        }
        Row {
            id: decision; anchors.right: parent.right; anchors.rightMargin: Design.space2xl; anchors.top: parent.top; anchors.topMargin: Design.spaceS; spacing: Design.spaceXs
            ActionButton { objectName: "workspaceThemeMakeDefault"; visible: picker.canMakeDefault; text: "Set as default"; onClicked: picker.makeDefaultRequested() }
            ActionButton { text: "Cancel"; onClicked: picker.close() }
            ActionButton { objectName: "workspaceThemeApply"; text: "Apply"; primary: true; onClicked: picker.applySelected() }
        }
    }
}
