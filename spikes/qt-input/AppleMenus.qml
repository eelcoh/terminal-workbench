import QtQuick
import QtQuick.Controls
import QtQml.Models

// The application menu as macOS menus in the system menu bar, grouped the way
// Mac applications group them. Every item runs a Commands entry, so titles and
// availability follow WorkbenchActions. Qt moves "About Band" and
// "Configuration…" into the Band menu by their titles and adds Hide and Quit.
// Items carry no key equivalents: the menu bar would take those keys before
// the terminal sees them, and Band's own shortcuts stay configurable.
MenuBar {
    id: bar
    required property var host
    required property var registry

    component CommandItem: MenuItem {
        required property string command
        readonly property var entry: bar.registry.find(command)
        text: entry ? entry.title : command
        enabled: !!entry && entry.available
        onTriggered: bar.host.dispatchCommand(command)
    }

    Menu {
        title: "Shell"
        CommandItem { command: "new-terminal" }
        CommandItem { command: "new-workspace" }
        Menu {
            id: hostMenu
            title: "Connect to host"
            Instantiator {
                model: bar.host.hostDefinitions
                delegate: CommandItem { required property var modelData; command: "host:" + modelData.id; text: modelData.name }
                onObjectAdded: (index, object) => hostMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => hostMenu.removeItem(object)
            }
            MenuSeparator {}
            CommandItem { command: "new-host" }
        }
        CommandItem { command: "apps" }
        MenuSeparator {}
        CommandItem { command: "session-start" }
        CommandItem { command: "session-stop" }
        MenuSeparator {}
        CommandItem { command: "close-terminal" }
        CommandItem { command: "close-workspace" }
        MenuItem { text: "Close window"; onTriggered: bar.host.close() }
    }
    Menu {
        title: "Edit"
        CommandItem { command: "copy" }
        CommandItem { command: "paste" }
        MenuSeparator {}
        CommandItem { command: "terminal-search" }
        MenuSeparator {}
        CommandItem { command: "forward" }
    }
    Menu {
        title: "View"
        CommandItem { command: "overview" }
        MenuSeparator {}
        CommandItem { command: "cycle-width" }
        CommandItem { command: "full-width" }
        MenuSeparator {}
        CommandItem { command: "font-increase" }
        CommandItem { command: "font-decrease" }
        CommandItem { command: "font-reset" }
        CommandItem { command: "font-settings" }
        MenuSeparator {}
        MenuItem { text: "Reduce motion"; checkable: true; checked: bar.host.reducedMotion; onTriggered: bar.host.motionOverride = checked }
    }
    Menu {
        title: "Workspace"
        CommandItem { command: "rename-workspace" }
        CommandItem { command: "workspace-folder" }
        CommandItem { command: "appearance" }
        MenuSeparator {}
        CommandItem { command: "move-left" }
        CommandItem { command: "move-right" }
        CommandItem { command: "move-up" }
        CommandItem { command: "move-down" }
    }
    Menu {
        title: "Go"
        CommandItem { command: "previous-terminal" }
        CommandItem { command: "next-terminal" }
        CommandItem { command: "previous-workspace" }
        CommandItem { command: "next-workspace" }
        MenuSeparator {}
        CommandItem { command: "next-waiting" }
        CommandItem { command: "previous-waiting" }
        CommandItem { command: "agents" }
        MenuSeparator {}
        CommandItem { command: "commands" }
    }
    Menu {
        title: "Help"
        CommandItem { command: "help" }
        CommandItem { command: "welcome" }
        MenuSeparator {}
        CommandItem { command: "terminal-info" }
        CommandItem { command: "agent-info" }
        MenuSeparator {}
        CommandItem { command: "configuration" }
        CommandItem { command: "configuration-reload" }
        CommandItem { command: "shortcut-settings" }
        CommandItem { command: "gallery" }
        MenuSeparator {}
        CommandItem { command: "about" }
    }
}
