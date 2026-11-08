import QtQuick
import QtQuick.Controls
import Band.Design
Dialog {
    id: picker
    objectName: "appearanceDialog"
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
    implicitWidth: 540; implicitHeight: 600
    modal: true; width: Math.min(implicitWidth, parent.width - 64); height: Math.min(implicitHeight, parent.height - 64)
    padding: Design.space2xl; closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "transparent" }
    background: Rectangle { color: "#1b2029"; radius: Design.radiusXl; border.color: "#3a4657" }
    header: Label { renderType: Text.NativeRendering; width: picker.width; text: picker.title; textFormat: Text.PlainText; padding: Design.space2xl; bottomPadding: Design.spaceM; color: "#e0e6ef"; font.pixelSize: Design.textTitle; elide: Text.ElideRight }
    onClosed: themeHelp.close()
    onAboutToShow: preparing = true
    onOpened: { search.text = ""; mode = 0; list.currentIndex = -1; search.forceActiveFocus(); preparing = false }
    function revealTheme(index) {
        search.text = ""; mode = 0
        list.currentIndex = matches.findIndex(theme => theme.index === index)
        if (list.currentIndex >= 0) list.positionViewAtIndex(list.currentIndex,ListView.Contain)
        list.forceActiveFocus()
    }
    component ActionButton: Button {
        id: action
        property bool primary: false
        implicitWidth: 100; implicitHeight: 36; hoverEnabled: true
        contentItem: Text { text: action.text; color: action.primary ? "#101318" : "#dce3ed"; font.pixelSize: Design.textBody; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: Design.radiusM; color: action.primary ? "#b4c9ec" : action.hovered || action.visualFocus ? "#343e4e" : "#252d38"; border.width: action.visualFocus ? Design.line : 0; border.color: "#b4c9ec" }
    }
    contentItem: Column {
        spacing: Design.spaceL
        TextField {
            id: search; objectName: "themeSearch"; width: parent.width; placeholderText: "Search themes"; selectByMouse: true
            color: "#e0e6ef"; selectionColor: "#b4c9ec"; selectedTextColor: "#101318"; placeholderTextColor: "#7d899c"; padding: Design.spaceL
            background: Rectangle { color: "#101318"; radius: Design.radiusM; border.color: search.activeFocus ? "#b4c9ec" : "#3a4657" }
            Keys.onDownPressed: if (list.count) { list.currentIndex = 0; list.forceActiveFocus(); picker.previewSelected(picker.matches[0].index) }
        }
        Item {
            id: modes; width: parent.width; height: 36
            Row {
                spacing: Design.spaceM
                Repeater {
                    model: ["All", "Dark", "Light"]
                    ActionButton { required property int index; required property string modelData; text: modelData; primary: picker.mode === index; onClicked: picker.mode = index }
                }
            }
            ToolButton {
                id: helpButton; objectName: "themeHelpButton"
                visible: picker.directoryPath.length > 0
                anchors.right: parent.right; width: 32; height: 32; hoverEnabled: true
                Accessible.name: "Help with themes"
                contentItem: Text { text: "?"; color: "#b4c9ec"; font.pixelSize: Design.textControl; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { anchors.centerIn: parent; width: 22; height: 22; radius: 11; color: helpButton.hovered ? "#343e4e" : "transparent"; border.color: helpButton.visualFocus ? "#b4c9ec" : "#66758b" }
                ToolTip.visible: hovered; ToolTip.text: "Help with themes"
                onClicked: themeHelp.open()
            }
        }
        Label { id: folderStatus; width: parent.width; visible: picker.directoryStatus.length > 0; text: picker.directoryStatus; textFormat: Text.PlainText; elide: Text.ElideRight; color: "#9caabd"; font.pixelSize: Design.textCaption }
        ListView {
            id: list; objectName: "themeList"; width: parent.width; height: Math.max(80, parent.height - search.height - modes.height - previewLabel.implicitHeight - sourceLabel.implicitHeight - (folderStatus.visible ? folderStatus.implicitHeight + parent.spacing : 0) - (saveErrorLabel.visible ? saveErrorLabel.implicitHeight + parent.spacing : 0) - (noteLabel.visible ? noteLabel.implicitHeight + parent.spacing : 0) - parent.spacing * 4)
            clip: true; spacing: Design.spaceM; model: picker.matches; currentIndex: -1
            ScrollBar.vertical: ScrollBar {}
            onCurrentIndexChanged: if (picker.visible && !picker.preparing && activeFocus && currentIndex >= 0 && currentIndex < count) picker.previewSelected(picker.matches[currentIndex].index)
            Keys.onReturnPressed: picker.applySelected()
            delegate: ItemDelegate {
                id: option; required property var modelData; required property int index
                width: list.width; height: 138
                Accessible.name: modelData.name + (modelData.light ? ", light" : ", dark")
                background: Rectangle { radius: Design.radiusM; color: "#141922"; border.width: Design.line; border.color: picker.themeIndex === option.modelData.index || option.activeFocus ? "#b4c9ec" : "#343e4e" }
                contentItem: Column {
                    spacing: Design.spaceM
                    Label { text: option.modelData.name; textFormat: Text.PlainText; color: "#e0e6ef"; font.pixelSize: Design.textBody }
                    Rectangle {
                        width: parent.width; height: 92; radius: Design.radiusS; color: option.modelData.background
                        Column {
                            anchors.fill: parent; anchors.margins: Design.spaceM; spacing: Design.spaceS
                            Text {
                                width: parent.width; textFormat: Text.PlainText; elide: Text.ElideRight
                                text: "~/project  $ git status"; color: option.modelData.foreground
                                font.family: "JetBrains Mono"; font.pixelSize: Design.textControl; renderType: Text.NativeRendering
                            }
                            Grid {
                                width: parent.width; columns: 8; spacing: Design.spaceXs
                                Repeater {
                                    model: option.modelData.swatches
                                    Rectangle { required property string modelData; width: (parent.width - 21) / 8; height: 22; radius: Design.radiusS; color: modelData }
                                }
                            }
                        }
                    }
                }
                onClicked: { list.currentIndex = index; picker.previewSelected(modelData.index) }
                Keys.onReturnPressed: picker.applySelected()
            }
            Label { anchors.centerIn: parent; visible: list.count === 0; text: "No matching themes"; color: "#9caabd" }
        }
        Label { id: saveErrorLabel; objectName: "workspaceAppearanceError"; width: parent.width; visible: picker.saveError.length > 0; text: picker.saveError; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#e6c689"; font.pixelSize: Design.textSmall }
        Label { id: previewLabel; width: parent.width; textFormat: Text.PlainText; text: "Previewing " + ((picker.catalog.find(theme => theme.index === picker.themeIndex) || picker.retainedTheme)?.name || "Current workspace theme") + " · " + picker.scope; color: "#9caabd"; font.pixelSize: Design.textSmall; elide: Text.ElideRight }
        Label { id: noteLabel; objectName: "appearanceNote"; width: parent.width; visible: picker.note.length > 0; text: picker.note; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: "#9caabd"; font.pixelSize: Design.textCaption }
        Label {
            id: sourceLabel; width: parent.width; color: "#9caabd"; font.pixelSize: Design.textCaption; textFormat: Text.PlainText; elide: Text.ElideMiddle
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
        background: Rectangle { color: "#252d38"; radius: Design.radiusL; border.color: "#66758b" }
        contentItem: Column {
            spacing: Design.spaceL
            Label { text: "Add your own themes"; color: "#e0e6ef"; font.pixelSize: Design.textControl }
            Label {
                width: parent.width; wrapMode: Text.WordWrap; color: "#bdc8d8"; font.pixelSize: Design.textSmall
                text: "Drop Ghostty theme files in this folder, including TerminalColors .default downloads. Reopen Appearance to see them in the list, then select a theme and Apply. No import or file opening is needed."
            }
            Label { width: parent.width; text: picker.directoryPath; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: "#9caabd"; font.pixelSize: Design.textCaption }
            Label {
                objectName: "themesFolderLink"
                text: '<a href="themes-folder" style="color: #b4c9ec">Open themes folder</a>'
                textFormat: Text.RichText; font.pixelSize: Design.textSmall
                Accessible.role: Accessible.Link; Accessible.name: "Open themes folder in file manager"
                activeFocusOnTab: true
                onLinkActivated: picker.folderRequested()
                Keys.onReturnPressed: picker.folderRequested()
                Keys.onSpacePressed: picker.folderRequested()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                Rectangle { anchors.fill: parent; anchors.margins: -2; visible: parent.activeFocus; color: "transparent"; radius: Design.radiusS; border.color: "#b4c9ec" }
            }
            ActionButton { text: "Got it"; onClicked: themeHelp.close() }
        }
    }
    footer: Item {
        implicitHeight: 92
        ActionButton { id: defaultButton; objectName: "workspaceThemeDefault"; x: 20; y: 0; implicitWidth: Math.max(100, contentItem.implicitWidth + 28); text: picker.followsDefault ? picker.usingDefaultText : picker.useDefaultText; enabled: !picker.followsDefault; onClicked: picker.defaultRequested() }
        Label { x: defaultButton.x + defaultButton.width + 12; y: 10; width: parent.width - x - 20; text: picker.defaultName; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: Design.textSmall; color: "#9caabd" }
        Row {
            anchors.right: parent.right; anchors.rightMargin: Design.space2xl; anchors.bottom: parent.bottom; anchors.bottomMargin: Design.spaceL; spacing: Design.spaceM
            ActionButton { objectName: "workspaceThemeMakeDefault"; visible: picker.canMakeDefault; implicitWidth: contentItem.implicitWidth + 28; text: "Set as default"; onClicked: picker.makeDefaultRequested() }
            ActionButton { text: "Cancel"; onClicked: picker.close() }
            ActionButton { objectName: "workspaceThemeApply"; text: "Apply"; primary: true; onClicked: picker.applySelected() }
        }
    }
}
