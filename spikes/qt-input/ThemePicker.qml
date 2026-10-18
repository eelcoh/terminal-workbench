import QtQuick
import QtQuick.Controls
Dialog {
    id: picker
    objectName: "appearanceDialog"
    property var catalog: []
    property int themeIndex: 0
    property string workspaceName: ""
    property int mode: 0
    property bool preparing: false
    property var retainedTheme: ({})
    property string directoryPath: ""
    property string directoryStatus: ""
    signal folderRequested()
    readonly property bool searchFocused: search.activeFocus
    signal previewSelected(int index)
    signal applySelected()
    readonly property var matches: catalog.filter(theme => theme.name.toLowerCase().includes(search.text.toLowerCase()) && (mode === 0 || theme.light === (mode === 2)))
    title: "Appearance · " + workspaceName
    implicitWidth: 540; implicitHeight: 600
    modal: true; width: Math.min(implicitWidth, parent.width - 64); height: Math.min(implicitHeight, parent.height - 64)
    padding: 20; closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
    header: Label { renderType: Text.NativeRendering; width: picker.width; text: picker.title; textFormat: Text.PlainText; padding: 20; bottomPadding: 8; color: "#e0e6ef"; font.pixelSize: 18; elide: Text.ElideRight }
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
        Row {
            id: modes; spacing: 8
            Repeater {
                model: ["All", "Dark", "Light"]
                ActionButton { required property int index; required property string modelData; text: modelData; primary: picker.mode === index; onClicked: picker.mode = index }
            }
        }
        Column {
            id: folderHint; width: parent.width; spacing: 4
            visible: picker.directoryPath.length > 0
            Label {
                width: parent.width; wrapMode: Text.WordWrap; color: "#9caabd"; font.pixelSize: 11
                text: "Themes in this folder appear in the list automatically. Add Ghostty files, then reopen Appearance."
            }
            Label {
                width: parent.width; text: picker.directoryPath; textFormat: Text.PlainText
                elide: Text.ElideMiddle; color: "#9caabd"; font.pixelSize: 11
                ToolTip.visible: folderPathHover.hovered; ToolTip.text: picker.directoryPath
                HoverHandler { id: folderPathHover }
            }
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
        }
        Label { id: folderStatus; width: parent.width; visible: picker.directoryStatus.length > 0; text: picker.directoryStatus; textFormat: Text.PlainText; elide: Text.ElideRight; color: "#9caabd"; font.pixelSize: 11 }
        ListView {
            id: list; objectName: "themeList"; width: parent.width; height: Math.max(80, parent.height - search.height - modes.height - previewLabel.implicitHeight - sourceLabel.implicitHeight - (folderHint.visible ? folderHint.implicitHeight + parent.spacing : 0) - (folderStatus.visible ? folderStatus.implicitHeight + parent.spacing : 0) - parent.spacing * 4)
            clip: true; spacing: 8; model: picker.matches; currentIndex: -1
            ScrollBar.vertical: ScrollBar {}
            onCurrentIndexChanged: if (picker.visible && !picker.preparing && activeFocus && currentIndex >= 0 && currentIndex < count) picker.previewSelected(picker.matches[currentIndex].index)
            Keys.onReturnPressed: picker.applySelected()
            delegate: ItemDelegate {
                id: option; required property var modelData; required property int index
                width: list.width; height: 68
                Accessible.name: modelData.name + (modelData.light ? ", light" : ", dark")
                background: Rectangle { radius: 7; color: "#141922"; border.width: 1; border.color: picker.themeIndex === option.modelData.index || option.activeFocus ? "#b4c9ec" : "#343e4e" }
                contentItem: Column {
                    spacing: 8
                    Label { text: option.modelData.name; textFormat: Text.PlainText; color: "#e0e6ef"; font.pixelSize: 13 }
                    Row { spacing: 4; Repeater { model: option.modelData.swatches; Rectangle { required property string modelData; width: 24; height: 9; radius: 3; color: modelData } } }
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
    footer: Item {
        implicitHeight: 56
        Row {
            anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter; spacing: 8
            ActionButton { text: "Cancel"; onClicked: picker.close() }
            ActionButton { text: "Apply"; primary: true; onClicked: picker.applySelected() }
        }
    }
}
