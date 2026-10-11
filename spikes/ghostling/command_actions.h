#ifndef WORKBENCH_COMMAND_ACTIONS_H
#define WORKBENCH_COMMAND_ACTIONS_H
typedef enum {
    COMMAND_OVERVIEW, COMMAND_APPEARANCE, COMMAND_WORKSPACE_CREATE,
    COMMAND_WORKSPACE_RENAME, COMMAND_WORKSPACE_FOLDER, COMMAND_WORKSPACE_STOP,
    COMMAND_TERMINAL_CREATE, COMMAND_TERMINAL_RENAME, COMMAND_TERMINAL_STOP,
    COMMAND_TERMINAL_PREVIOUS, COMMAND_TERMINAL_NEXT,
    COMMAND_WORKSPACE_PREVIOUS, COMMAND_WORKSPACE_NEXT, COMMAND_TERMINAL_RECOVER,
    COMMAND_PALETTE, COMMAND_COPY, COMMAND_PASTE, COMMAND_FORWARD, COMMAND_SHORTCUTS,
    COMMAND_COUNT
} WorkbenchCommand;
typedef struct { const char *title, *detail; } CommandDefinition;
static const CommandDefinition command_definitions[COMMAND_COUNT]={
    {"Open overview","Search workspaces and terminals"},
    {"Change appearance","Themes and application colors"},
    {"New workspace","Create a project and its first shell"},
    {"Rename workspace","Edit this workspace's name"},
    {"Change project folder","Where new and replacement shells start"},
    {"Stop workspace","Confirm before stopping its shells"},
    {"New terminal","Create a shell in this workspace"},
    {"Rename terminal","Edit the focused terminal's name"},
    {"Stop terminal","Confirm before stopping the focused shell"},
    {"Previous terminal","Focus the preceding terminal"},
    {"Next terminal","Focus the following terminal"},
    {"Previous workspace","Switch to the preceding workspace"},
    {"Next workspace","Switch to the following workspace"},
    {"Start new shell","Confirm replacement of a stopped terminal"},
    {"Open Commands","Search application actions"},
    {"Copy selection","Copy text from the focused terminal"},
    {"Paste clipboard","Paste into the focused terminal"},
    {"Send next key to terminal","Bypass application shortcuts once"},
    {"Shortcut settings","Change bindings and navigation shortcuts"}
};
static const char *command_ids[COMMAND_COUNT]={
    "overview", "appearance", "workspace.new", "workspace.rename", "workspace.folder", "workspace.stop",
    "terminal.new", "terminal.rename", "terminal.stop", "terminal.previous", "terminal.next",
    "workspace.previous", "workspace.next", "terminal.recover", "commands", "copy", "paste", "forward", "shortcuts"
};
#endif
