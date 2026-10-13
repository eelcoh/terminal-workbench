/* Client-only help groups and bounded scroll geometry, independent of rendering. */
#ifndef WORKBENCH_SHORTCUT_HELP_H
#define WORKBENCH_SHORTCUT_HELP_H
#include "command_actions.h"
#include <math.h>
#include <stdbool.h>
#define SHORTCUT_HELP_GROUPS 6
typedef struct { const char *title; int count, actions[8]; } ShortcutHelpGroup;
static const ShortcutHelpGroup shortcut_help_groups[SHORTCUT_HELP_GROUPS]={
    {"Navigation",4,{COMMAND_TERMINAL_PREVIOUS,COMMAND_TERMINAL_NEXT,COMMAND_WORKSPACE_PREVIOUS,COMMAND_WORKSPACE_NEXT}},
    {"Terminals & workspaces",8,{COMMAND_TERMINAL_CREATE,COMMAND_WORKSPACE_CREATE,COMMAND_TERMINAL_RENAME,COMMAND_WORKSPACE_RENAME,
        COMMAND_WORKSPACE_FOLDER,COMMAND_TERMINAL_STOP,COMMAND_WORKSPACE_STOP,COMMAND_TERMINAL_RECOVER}},
    {"Terminal widths",5,{COMMAND_WIDTH_CYCLE,COMMAND_WIDTH_FULL,COMMAND_WIDTH_THIRD,COMMAND_WIDTH_HALF,COMMAND_WIDTH_TWO_THIRDS}},
    {"Overview & appearance",6,{COMMAND_OVERVIEW,COMMAND_APPEARANCE,COMMAND_FONT_SETTINGS,COMMAND_FONT_INCREASE,COMMAND_FONT_DECREASE,COMMAND_FONT_RESET}},
    {"Clipboard & input",3,{COMMAND_COPY,COMMAND_PASTE,COMMAND_FORWARD}},
    {"Commands & help",3,{COMMAND_PALETTE,COMMAND_HELP,COMMAND_SHORTCUTS}}
};
static float shortcut_help_group_height(int group) { return 46+shortcut_help_groups[group].count*32; }
static float shortcut_help_group_top(int group, bool wide)
{
    float y=0;
    for (int i=wide ? group%2 : 0;i<group;i+=wide ? 2 : 1) y+=shortcut_help_group_height(i)+12;
    return y;
}
static float shortcut_help_content_height(bool wide)
{
    float height=0;
    for (int g=0;g<SHORTCUT_HELP_GROUPS;g++) height=fmaxf(height,shortcut_help_group_top(g,wide)+shortcut_help_group_height(g));
    return height;
}
static float shortcut_help_scroll(float position, float delta, float maximum)
{
    if (!isfinite(position)) position=0;
    if (!isfinite(delta)) delta=0;
    return fminf(fmaxf(0,maximum),fmaxf(0,position+delta));
}
#endif
