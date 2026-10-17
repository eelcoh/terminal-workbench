import QtQuick
import QtQuick.Controls
Dialog {
    id: picker
    objectName: "appearanceDialog"
    property var catalog: []
    property int themeIndex: 0
    property string workspaceName: ""
    property int mode: 0
    readonly property bool searchFocused: search.activeFocus
    signal previewSelected(int index)
    signal applySelected()
    readonly property var matches: catalog.filter(theme => theme.name.toLowerCase().includes(search.text.toLowerCase()) && (mode === 0 || theme.light === (mode === 2)))
    title: "Appearance · " + workspaceName
    implicitWidth: 540; implicitHeight: 600
    modal: true; width: Math.min(implicitWidth, parent.width - 64); height: Math.min(implicitHeight, parent.height - 64)
    padding: 20; closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
    header: Label { renderType: Text.NativeRendering; width: picker.width; text: picker.title; padding: 20; bottomPadding: 8; color: "#e0e6ef"; font.pixelSize: 18; elide: Text.ElideRight }
    onOpened: { search.text = ""; mode = 0; list.currentIndex = -1; search.forceActiveFocus() }
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
        ListView {
            id: list; objectName: "themeList"; width: parent.width; height: Math.max(80, parent.height - search.height - modes.height - previewLabel.implicitHeight - parent.spacing * 3)
            clip: true; spacing: 8; model: picker.matches; currentIndex: -1
            ScrollBar.vertical: ScrollBar {}
            onCurrentIndexChanged: if (activeFocus && currentIndex >= 0 && currentIndex < count) picker.previewSelected(picker.matches[currentIndex].index)
            Keys.onReturnPressed: picker.applySelected()
            delegate: ItemDelegate {
                id: option; required property var modelData; required property int index
                width: list.width; height: 68
                Accessible.name: modelData.name + (modelData.light ? ", light" : ", dark")
                background: Rectangle { radius: 7; color: "#141922"; border.width: 1; border.color: picker.themeIndex === option.modelData.index || option.activeFocus ? "#b4c9ec" : "#343e4e" }
                contentItem: Column {
                    spacing: 8
                    Label { text: option.modelData.name; color: "#e0e6ef"; font.pixelSize: 13 }
                    Row { spacing: 4; Repeater { model: option.modelData.swatches; Rectangle { required property string modelData; width: 24; height: 9; radius: 3; color: modelData } } }
                }
                onClicked: { list.currentIndex = index; picker.previewSelected(modelData.index) }
                Keys.onReturnPressed: picker.applySelected()
            }
            Label { anchors.centerIn: parent; visible: list.count === 0; text: "No matching themes"; color: "#9caabd" }
        }
        Label { id: previewLabel; width: parent.width; text: "Previewing " + (picker.catalog[picker.themeIndex]?.name || "") + " · all terminals in this workspace"; color: "#9caabd"; font.pixelSize: 12; elide: Text.ElideRight }
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
