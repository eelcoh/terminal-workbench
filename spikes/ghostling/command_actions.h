#ifndef WORKBENCH_COMMAND_ACTIONS_H
#define WORKBENCH_COMMAND_ACTIONS_H
typedef enum {
    COMMAND_OVERVIEW, COMMAND_APPEARANCE, COMMAND_WORKSPACE_CREATE,
    COMMAND_WORKSPACE_RENAME, COMMAND_WORKSPACE_FOLDER, COMMAND_WORKSPACE_STOP,
    COMMAND_TERMINAL_CREATE, COMMAND_TERMINAL_RENAME, COMMAND_TERMINAL_STOP,
    COMMAND_TERMINAL_PREVIOUS, COMMAND_TERMINAL_NEXT,
    COMMAND_WORKSPACE_PREVIOUS, COMMAND_WORKSPACE_NEXT, COMMAND_TERMINAL_RECOVER,
    COMMAND_PALETTE, COMMAND_COPY, COMMAND_PASTE, COMMAND_FORWARD, COMMAND_SHORTCUTS,
    COMMAND_WIDTH_CYCLE, COMMAND_WIDTH_FULL, COMMAND_WIDTH_THIRD, COMMAND_WIDTH_HALF, COMMAND_WIDTH_TWO_THIRDS,
    COMMAND_HELP,
    COMMAND_FONT_SETTINGS, COMMAND_FONT_INCREASE, COMMAND_FONT_DECREASE, COMMAND_FONT_RESET,
    COMMAND_SEARCH, COMMAND_SESSION_STOP,
    COMMAND_AGENT_INPUT_NEXT, COMMAND_AGENT_INPUT_PREVIOUS,
    COMMAND_COUNT
} WorkbenchCommand;
typedef struct { const char *title, *detail; } CommandDefinition;
static const CommandDefinition command_definitions[COMMAND_COUNT]={
    {"Open overview","Search workspaces and terminals"},
    {"Change appearance","Choose a theme for the active workspace"},
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
    {"Shortcut settings","Change bindings and navigation shortcuts"},
    {"Cycle terminal width","Half, two thirds, one third"},
    {"Toggle terminal full width","Fill the terminal area; toggle again to restore"},
    {"Terminal width: one third","Fit three terminals side by side"},
    {"Terminal width: one half","Fit two terminals side by side"},
    {"Terminal width: two thirds","Pair with a one-third terminal"},
    {"Show shortcuts","View current bindings and open Shortcut settings"},
    {"Terminal text settings","Choose a font and size for this window"},
    {"Increase terminal text size","Larger terminal text; keep interface size"},
    {"Decrease terminal text size","Smaller terminal text; keep interface size"},
    {"Reset terminal text size","Use the configured size, retaining the selected font"},
    {"Find in terminal","Search retained output and scrollback in the focused pane"},
    {"Stop all terminals","Confirm shutdown across windows; keep workspaces for restart"},
    {"Next agent needing input","Focus the next terminal whose last report needs input"},
    {"Previous agent needing input","Focus the previous terminal whose last report needs input"}
};
static const char *command_ids[COMMAND_COUNT]={
    "overview", "appearance", "workspace.new", "workspace.rename", "workspace.folder", "workspace.stop",
    "terminal.new", "terminal.rename", "terminal.stop", "terminal.previous", "terminal.next",
    "workspace.previous", "workspace.next", "terminal.recover", "commands", "copy", "paste", "forward", "shortcuts", "terminal.width.cycle", "terminal.width.full", "terminal.width.third", "terminal.width.half", "terminal.width.two-thirds", "shortcuts.help", "terminal.font.settings", "terminal.font.increase", "terminal.font.decrease", "terminal.font.reset", "terminal.search", "session.stop", "agent.input.next", "agent.input.previous"
};
#endif
