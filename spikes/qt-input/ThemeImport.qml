import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as NativeDialogs
Dialog {
    id: form
    objectName: "themeImportDialog"
    property var themes
    property var session
    property var browser: null
    readonly property bool browserOpen: browser !== null && browser.visible
    readonly property string sourceText: source.text
    readonly property bool sourceFocused: source.activeFocus
    title: "Import theme"
    implicitWidth: 540; width: Math.min(implicitWidth,parent.width - 64)
    modal: true; anchors.centerIn: parent; padding: 20
    closePolicy: browserOpen ? Popup.NoAutoClose : Popup.CloseOnEscape
    background: Rectangle { color: "#1b2029"; radius: 10; border.color: "#3a4657" }
    header: Label { text: form.title; textFormat: Text.PlainText; padding: 20; bottomPadding: 8; color: "#e0e6ef"; font.pixelSize: 18 }
    component Field: TextField {
        width: parent.width; selectByMouse: true; padding: 10
        color: "#e0e6ef"; selectionColor: "#b4c9ec"; selectedTextColor: "#101318"; placeholderTextColor: "#7d899c"
        background: Rectangle { radius: 6; color: "#101318"; border.color: parent.activeFocus ? "#b4c9ec" : "#3a4657" }
    }
    component ActionButton: Button {
        id: button
        focusPolicy: Qt.StrongFocus; implicitHeight: 36; implicitWidth: contentItem.implicitWidth + 22; hoverEnabled: true
        contentItem: Text { text: button.text; textFormat: Text.PlainText; color: "#dce3ed"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 6; color: button.hovered || button.visualFocus ? "#343e4e" : "#252d38"; border.width: button.visualFocus ? 1 : 0; border.color: "#b4c9ec" }
    }
    onOpened: { source.text = ""; name.text = ""; attribution.text = ""; themes.clearError(); source.forceActiveFocus() }
    onClosed: { themes.cancelImport(); if (browser) browser.close() }
    function restoreSourceFocus() { if (visible) source.forceActiveFocus() }
    function browse() {
        if (browser) browser.destroy()
        const local = source.text.includes("://") ? "" : source.text
        browser = browserFactory.createObject(form, {currentFolder: session.folderStart(local)})
        if (browser) browser.open()
    }
    contentItem: Column {
        spacing: 10
        Label { text: "Ghostty file or TerminalColors link"; color: "#c2c9d4" }
        Row {
            width: parent.width; spacing: 8
            Field { id: source; objectName: "themeImportSource"; width: parent.width - browseButton.width - parent.spacing; placeholderText: "File path or https://terminalcolors.com/themes/…"; maximumLength: 4096; enabled: !form.themes.busy; onAccepted: if (submit.enabled) submit.clicked() }
            ActionButton { id: browseButton; objectName: "themeImportBrowse"; text: "Browse…"; enabled: !form.themes.busy; onClicked: form.browse() }
        }
        Label { text: "Name (optional)"; color: "#c2c9d4" }
        Field { id: name; objectName: "themeImportName"; maximumLength: 120; placeholderText: "Use the file or theme name"; enabled: !form.themes.busy; onAccepted: if (submit.enabled) submit.clicked() }
        Label { text: "Author / license notes (optional)"; color: "#c2c9d4" }
        Field { id: attribution; objectName: "themeImportAttribution"; maximumLength: 1024; enabled: !form.themes.busy; onAccepted: if (submit.enabled) submit.clicked() }
        Label { width: parent.width; text: "Imports are saved offline. Import previews the current workspace; Apply in Appearance keeps its new look."; wrapMode: Text.Wrap; color: "#9caabd"; font.pixelSize: 12 }
        Label {
            text: "Find Ghostty themes at <a href='https://terminalcolors.com/ghostty/'>terminalcolors.com</a>"; color: "#9caabd"; linkColor: "#b4c9ec"; font.pixelSize: 12
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
        Label { width: parent.width; text: form.themes.error; textFormat: Text.PlainText; visible: text.length > 0; wrapMode: Text.Wrap; color: "#e3bf91"; font.pixelSize: 12 }
        Row {
            spacing: 8
            ActionButton { text: "Cancel"; onClicked: form.close() }
            ActionButton { id: submit; objectName: "themeImportSubmit"; text: form.themes.busy ? "Downloading…" : "Import & preview"; enabled: !form.themes.busy && source.text.length > 0; onClicked: form.themes.importTheme(source.text,name.text,attribution.text) }
        }
    }
    Component {
        id: browserFactory
        NativeDialogs.FileDialog {
            id: browserDialog
            parentWindow: form.parent.Window.window
            title: "Choose Ghostty theme"; fileMode: NativeDialogs.FileDialog.OpenFile; acceptLabel: "Choose theme"
            nameFilters: ["Theme files (*.conf *.ghostty *.txt)","All files (*)"]
            options: testFolderDialogs ? NativeDialogs.FileDialog.DontUseNativeDialog : 0
            Component.onCompleted: if (testFolderDialogs && "popupType" in browserDialog) browserDialog.popupType = Popup.Item
            onAccepted: { if (form.visible) { source.text = form.session.folderPath(selectedFile); source.deselect(); source.cursorPosition = source.text.length }; Qt.callLater(form.restoreSourceFocus) }
            onRejected: Qt.callLater(form.restoreSourceFocus)
        }
    }
}
