import QtQuick
import QtQuick.Controls
import Workbench.Probe

// First run in the main area: how Band looks, apps to open, and the keys worth
// knowing. Nothing here writes dotfiles; choices use the window's own view
// settings, and picked apps open as panes when it finishes.
FocusScope {
    id: welcome
    required property var host
    property var tones
    property int step: 0
    // App ID → true for the apps to open on finishing.
    property var picked: ({})
    readonly property var titles: ["Make Band yours", "Your apps", "Keys worth knowing"]
    readonly property var subtitles: [
        "Band keeps a strip of terminals per workspace. Scroll sideways, open apps beside your shells, and keep agents in view.",
        "Apps are terminal programs that open as panes next to your shells. These are installed here; pick any to open now.",
        "Everything is also in Commands. Change any binding in Shortcut settings."
    ]
    readonly property var installed: studyApps.entries.filter(entry => entry.installed === 1)
    readonly property var keys: [
        {id: "commands", title: "Open Commands"}, {id: "new-terminal", title: "New terminal"},
        {id: "next-terminal", title: "Next terminal"}, {id: "previous-terminal", title: "Previous terminal"},
        {id: "overview", title: "Overview of all workspaces"}, {id: "apps", title: "Running apps"}, {id: "help", title: "All keyboard shortcuts"}
    ].filter(entry => (studyShortcuts.labels[entry.id] || "").length > 0)
    signal finished(var apps, bool browse)
    function pickedApps() { return installed.filter(entry => picked[entry.id]).map(entry => entry.id) }
    function toggle(id) { let values = Object.assign({}, picked); values[id] = !values[id]; picked = values }
    function next() { if (step < titles.length - 1) step += 1; else finished(pickedApps(), false) }
    Keys.onReturnPressed: next()
    Keys.onEnterPressed: next()
    Keys.onEscapePressed: finished([], false)
    Keys.onLeftPressed: if (step > 0) step -= 1
    Keys.onRightPressed: next()

    Rectangle { anchors.fill: parent; color: welcome.tones.background }
    // Pointer input stays in the welcome instead of reaching the strip below.
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onPressed: welcome.forceActiveFocus() }

    Column {
        id: body; objectName: "welcomeBody"
        width: Math.min(620, parent.width - 64); x: (parent.width - width) / 2; y: Math.max(32, parent.height * 0.16)
        spacing: 18
        Row {
            spacing: 6
            Repeater {
                model: welcome.titles.length
                Rectangle { required property int index; width: index === welcome.step ? 22 : 8; height: 8; radius: 4; color: index === welcome.step ? welcome.tones.accent : welcome.tones.border }
            }
        }
        Column {
            width: parent.width; spacing: 6
            Text { objectName: "welcomeTitle"; text: welcome.titles[welcome.step]; color: welcome.tones.foreground; font.pixelSize: 28; font.weight: Font.DemiBold }
            Text { width: parent.width; text: welcome.subtitles[welcome.step]; wrapMode: Text.Wrap; color: welcome.tones.muted; font.pixelSize: 14; lineHeight: 1.15 }
        }

        // Step 1: the window's own view choices and the colors picker.
        Column {
            width: parent.width; spacing: 14; visible: welcome.step === 0
            component Choice: Row {
                id: choice
                property string label
                property var options: []
                property int current: 0
                signal chosen(int index)
                spacing: 8
                Text { width: 150; anchors.verticalCenter: parent.verticalCenter; text: choice.label; color: welcome.tones.foreground; font.pixelSize: 14 }
                Repeater {
                    model: choice.options
                    ChromeButton { required property int index; required property string modelData; tones: welcome.tones; text: modelData; selected: choice.current === index; focusPolicy: Qt.NoFocus; onClicked: choice.chosen(index) }
                }
            }
            Choice {
                objectName: "welcomeSidebar"; label: "Sidebar"; options: ["Pinned", "Opens when needed"]; current: welcome.host.navigationPinned ? 0 : 1
                onChosen: index => welcome.host.navigationOverride = index === 0
            }
            Choice {
                objectName: "welcomeMotion"; label: "Motion"; options: ["Animations", "Reduced motion"]; current: welcome.host.reducedMotion ? 1 : 0
                onChosen: index => welcome.host.motionOverride = index === 1
            }
            Text { text: "Colors"; color: welcome.tones.foreground; font.pixelSize: 14 }
            // Built-in themes as cards; choosing one saves Band's default theme.
            Flow {
                width: parent.width; spacing: 8
                Repeater {
                    model: studyThemes.catalog.filter(theme => !theme.imported && !theme.source)
                    Rectangle {
                        id: card
                        required property var modelData
                        required property int index
                        readonly property bool current: studyConfiguration.themeIndex === modelData.index
                        objectName: "welcomeTheme" + index
                        width: 146; height: 74; radius: 8; color: modelData.background
                        border.width: current ? 2 : 1; border.color: current ? welcome.tones.accent : welcome.tones.border
                        Accessible.role: Accessible.Button; Accessible.name: modelData.name + (current ? ", default" : "")
                        Text { x: 10; y: 10; width: parent.width - 20; elide: Text.ElideRight; text: card.modelData.name; textFormat: Text.PlainText; color: card.modelData.foreground; font.pixelSize: 12; font.weight: Font.DemiBold }
                        Row {
                            x: 10; y: 44; spacing: 4
                            Repeater { model: card.modelData.swatches.slice(1, 7); Rectangle { required property string modelData; width: 14; height: 14; radius: 3; color: modelData } }
                        }
                        ChromeIcon { anchors.right: parent.right; anchors.rightMargin: 8; y: 8; width: 16; height: 16; name: "check"; color: card.modelData.accent; visible: card.current }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: studyAppearance.saveDefault(card.modelData.index) }
                    }
                }
            }
            Row {
                spacing: 8
                ChromeButton { objectName: "welcomeColors"; tones: welcome.tones; text: "More colors…"; hint: "Themes from your themes folder and imports, for this workspace"; focusPolicy: Qt.NoFocus; enabled: welcome.host.spaces.length > 0; onClicked: welcome.host.openAppearance() }
                ChromeButton { objectName: "welcomeUseConfigured"; tones: welcome.tones; text: "Use config.conf"; visible: studyAppearance.saved; hint: "Remove Band's choice; terminal-theme in config.conf applies again"; focusPolicy: Qt.NoFocus; onClicked: studyAppearance.useConfigured() }
            }
            Text {
                width: parent.width; wrapMode: Text.Wrap; color: welcome.tones.muted; font.pixelSize: 12
                text: "The theme you choose is the default for new workspaces and those without their own colors. Band saves it beside your font and shortcut settings; it takes precedence over terminal-theme in config.conf."
            }
        }

        // Step 2: installed apps to open now.
        Column {
            width: parent.width; spacing: 4; visible: welcome.step === 1
            Text {
                visible: welcome.installed.length === 0; width: parent.width; wrapMode: Text.Wrap; color: welcome.tones.muted; font.pixelSize: 14
                text: studyApps.checking ? "Checking which apps are installed…" : "None of the catalog’s apps are installed yet."
            }
            ListView {
                id: appList; width: parent.width; height: Math.min(contentHeight, 260); clip: true; spacing: 2; interactive: contentHeight > height
                model: welcome.installed
                ScrollBar.vertical: ScrollBar {}
                delegate: ChromeButton {
                    required property var modelData
                    required property int index
                    objectName: "welcomeApp" + index; width: appList.width; height: 40; tones: welcome.tones; focusPolicy: Qt.NoFocus
                    selected: !!welcome.picked[modelData.id]; hint: modelData.name
                    contentItem: Item {
                        Rectangle {
                            x: 8; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; radius: 4
                            color: "transparent"; border.width: 1; border.color: welcome.picked[modelData.id] ? welcome.tones.accent : welcome.tones.border
                            ChromeIcon { anchors.centerIn: parent; width: 14; height: 14; name: "check"; color: welcome.tones.accent; visible: !!welcome.picked[modelData.id] }
                        }
                        Text { x: 34; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; textFormat: Text.PlainText; color: welcome.tones.foreground; font.pixelSize: 14 }
                        Text {
                            anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: Math.min(implicitWidth, parent.width * 0.55)
                            horizontalAlignment: Text.AlignRight; elide: Text.ElideRight; text: modelData.description || modelData.command; textFormat: Text.PlainText; color: welcome.tones.muted; font.pixelSize: 12
                        }
                    }
                    onClicked: welcome.toggle(modelData.id)
                }
            }
            ChromeButton { objectName: "welcomeBrowse"; tones: welcome.tones; text: "Browse the catalog…"; focusPolicy: Qt.NoFocus; onClicked: welcome.finished(welcome.pickedApps(), true) }
        }

        // Step 3: a few bindings from the live keymap.
        Column {
            width: parent.width; spacing: 2; visible: welcome.step === 2
            Repeater {
                model: welcome.keys
                Item {
                    required property var modelData
                    width: parent.width; height: 34
                    Accessible.role: Accessible.StaticText; Accessible.name: modelData.title + ", " + studyShortcuts.labels[modelData.id]
                    Text { x: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.title; color: welcome.tones.foreground; font.pixelSize: 14 }
                    KeyChips { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; keys: studyShortcuts.labels[modelData.id] || ""; tones: welcome.tones }
                }
            }
        }

        Item {
            width: parent.width; height: 40
            ChromeButton { objectName: "welcomeBack"; tones: welcome.tones; text: "Back"; visible: welcome.step > 0; focusPolicy: Qt.NoFocus; anchors.verticalCenter: parent.verticalCenter; onClicked: welcome.step -= 1 }
            Row {
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 8
                ChromeButton { objectName: "welcomeSkip"; tones: welcome.tones; text: "Skip"; hint: "Close the welcome; Commands can open it again"; focusPolicy: Qt.NoFocus; onClicked: welcome.finished([], false) }
                ChromeButton {
                    objectName: "welcomeContinue"; tones: welcome.tones; primary: true; focusPolicy: Qt.NoFocus; keys: "Enter"
                    text: welcome.step < welcome.titles.length - 1 ? "Continue" : welcome.pickedApps().length ? "Open " + welcome.pickedApps().length + (welcome.pickedApps().length === 1 ? " app" : " apps") : "Start using Band"
                    onClicked: welcome.next()
                }
            }
        }
    }
}
