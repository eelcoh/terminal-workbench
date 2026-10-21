import QtQuick
import QtQuick.Controls
Dialog {
    id: picker
    objectName: "appearanceDialog"
    property var catalog: []
    property int themeIndex: 0
    property bool followsDefault: false
    property string defaultName: ""
    signal defaultRequested()
    property string workspaceName: ""
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
    title: "Appearance · " + workspaceName
    implicitWidth: 540; implicitHeight: 600
    modal: true; width: Math.min(implicitWidth, parent.width - 64); height: Math.min(implicitHeight, parent.height - 64)
    padding: 20; closePolicy: Popup.CloseOnEscape
    Overlay.modal: Rectangle { color: "transparent" }
    background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
    header: Label { renderType: Text.NativeRendering; width: picker.width; text: picker.title; textFormat: Text.PlainText; padding: 20; bottomPadding: 8; color: "#e0e6ef"; font.pixelSize: 18; elide: Text.ElideRight }
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
        contentItem: Text { text: action.text; color: action.primary ? "#101318" : "#dce3ed"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 6; color: action.primary ? "#b4c9ec" : action.hovered || action.visualFocus ? "#343e4e" : "#252d38"; border.width: action.visualFocus ? 1 : 0; border.color: "#b4c9ec" }
    }
    contentItem: Column {
        spacing: 12
        TextField {
            id: search; objectName: "themeSearch"; width: parent.width; placeholderText: "Search themes"; selectByMouse: true
            color: "#e0e6ef"; selectionColor: "#b4c9ec"; selectedTextColor: "#101318"; placeholderTextColor: "#7d899c"; padding: 10
            background: Rectangle { color: "#101318"; radius: 6; border.color: search.activeFocus ? "#b4c9ec" : "#3a4657" }
            Keys.onDownPressed: if (list.count) { list.currentIndex = 0; list.forceActiveFocus(); picker.previewSelected(picker.matches[0].index) }
        }
        Item {
            id: modes; width: parent.width; height: 36
            Row {
                spacing: 8
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
                contentItem: Text { text: "?"; color: "#b4c9ec"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { anchors.centerIn: parent; width: 22; height: 22; radius: 11; color: helpButton.hovered ? "#343e4e" : "transparent"; border.color: helpButton.visualFocus ? "#b4c9ec" : "#66758b" }
                ToolTip.visible: hovered; ToolTip.text: "Help with themes"
                onClicked: themeHelp.open()
            }
        }
        Label { id: folderStatus; width: parent.width; visible: picker.directoryStatus.length > 0; text: picker.directoryStatus; textFormat: Text.PlainText; elide: Text.ElideRight; color: "#9caabd"; font.pixelSize: 11 }
        ListView {
            id: list; objectName: "themeList"; width: parent.width; height: Math.max(80, parent.height - search.height - modes.height - previewLabel.implicitHeight - sourceLabel.implicitHeight - (folderStatus.visible ? folderStatus.implicitHeight + parent.spacing : 0) - parent.spacing * 4)
            clip: true; spacing: 8; model: picker.matches; currentIndex: -1
            ScrollBar.vertical: ScrollBar {}
            onCurrentIndexChanged: if (picker.visible && !picker.preparing && activeFocus && currentIndex >= 0 && currentIndex < count) picker.previewSelected(picker.matches[currentIndex].index)
            Keys.onReturnPressed: picker.applySelected()
            delegate: ItemDelegate {
                id: option; required property var modelData; required property int index
                width: list.width; height: 138
                Accessible.name: modelData.name + (modelData.light ? ", light" : ", dark")
                background: Rectangle { radius: 7; color: "#141922"; border.width: 1; border.color: picker.themeIndex === option.modelData.index || option.activeFocus ? "#b4c9ec" : "#343e4e" }
                contentItem: Column {
                    spacing: 7
                    Label { text: option.modelData.name; textFormat: Text.PlainText; color: "#e0e6ef"; font.pixelSize: 13 }
                    Rectangle {
                        width: parent.width; height: 92; radius: 4; color: option.modelData.background
                        Column {
                            anchors.fill: parent; anchors.margins: 8; spacing: 6
                            Text {
                                width: parent.width; textFormat: Text.PlainText; elide: Text.ElideRight
                                text: "~/project  $ git status"; color: option.modelData.foreground
                                font.family: "monospace"; font.pixelSize: 14; renderType: Text.NativeRendering
                            }
                            Grid {
                                width: parent.width; columns: 8; spacing: 3
                                Repeater {
                                    model: option.modelData.swatches
                                    Rectangle { required property string modelData; width: (parent.width - 21) / 8; height: 22; radius: 2; color: modelData }
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
        Label { id: previewLabel; width: parent.width; textFormat: Text.PlainText; text: "Previewing " + ((picker.catalog.find(theme => theme.index === picker.themeIndex) || picker.retainedTheme)?.name || "Current workspace theme") + " · all terminals in this workspace"; color: "#9caabd"; font.pixelSize: 12; elide: Text.ElideRight }
        Label {
            id: sourceLabel; width: parent.width; color: "#9caabd"; font.pixelSize: 11; textFormat: Text.PlainText; elide: Text.ElideMiddle
            readonly property var theme: picker.catalog.find(value => value.index === picker.themeIndex) || picker.retainedTheme
            text: theme?.imported ? "Source: " + theme.source + (theme.attribution ? " · " + theme.attribution : "") : "Bundled offline palette"
        }
    }
    Popup {
        id: themeHelp; objectName: "themeHelpPopup"
        parent: picker.contentItem; anchors.centerIn: parent
        width: Math.min(380, parent.width); padding: 16
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal: Rectangle { color: "transparent" }
        background: Rectangle { color: "#252d38"; radius: 8; border.color: "#66758b" }
        contentItem: Column {
            spacing: 10
            Label { text: "Add your own themes"; color: "#e0e6ef"; font.pixelSize: 14 }
            Label {
                width: parent.width; wrapMode: Text.WordWrap; color: "#bdc8d8"; font.pixelSize: 12
                text: "Drop Ghostty theme files in this folder, including TerminalColors .default downloads. Reopen Appearance to see them in the list, then select a theme and Apply. No import or file opening is needed."
            }
            Label { width: parent.width; text: picker.directoryPath; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: "#9caabd"; font.pixelSize: 11 }
            Label {
                objectName: "themesFolderLink"
                text: '<a href="themes-folder" style="color: #b4c9ec">Open themes folder</a>'
                textFormat: Text.RichText; font.pixelSize: 12
                Accessible.role: Accessible.Link; Accessible.name: "Open themes folder in file manager"
                activeFocusOnTab: true
                onLinkActivated: picker.folderRequested()
                Keys.onReturnPressed: picker.folderRequested()
                Keys.onSpacePressed: picker.folderRequested()
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                Rectangle { anchors.fill: parent; anchors.margins: -2; visible: parent.activeFocus; color: "transparent"; radius: 3; border.color: "#b4c9ec" }
            }
            ActionButton { text: "Got it"; onClicked: themeHelp.close() }
        }
    }
    footer: Item {
        implicitHeight: 92
        ActionButton { objectName: "workspaceThemeDefault"; x: 20; y: 0; text: picker.followsDefault ? "Using default" : "Use default"; enabled: !picker.followsDefault; onClicked: picker.defaultRequested() }
        Label { x: 132; y: 10; width: parent.width - 152; text: picker.defaultName; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 12; color: "#9caabd" }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 8
            ActionButton { text: "Cancel"; onClicked: picker.close() }
            ActionButton { objectName: "workspaceThemeApply"; text: "Apply"; primary: true; onClicked: picker.applySelected() }
        }
    }
}
