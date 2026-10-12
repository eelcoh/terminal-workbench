#include <assert.h>
#include "shortcut_help.h"
int main(void)
{
    int counts[COMMAND_COUNT]={0};
    for (int g=0;g<SHORTCUT_HELP_GROUPS;g++) {
        assert(shortcut_help_groups[g].title && shortcut_help_groups[g].count>0 && shortcut_help_groups[g].count<=8);
        for (int i=0;i<shortcut_help_groups[g].count;i++) {
            int action=shortcut_help_groups[g].actions[i];assert(action>=0 && action<COMMAND_COUNT);
            assert(command_ids[action][0] && command_definitions[action].title[0] && command_definitions[action].detail[0]);counts[action]++;
        }
        if (g) assert(shortcut_help_group_top(g,false)==shortcut_help_group_top(g-1,false)+shortcut_help_group_height(g-1)+12);
        if (g>=2) assert(shortcut_help_group_top(g,true)==shortcut_help_group_top(g-2,true)+shortcut_help_group_height(g-2)+12);
    }
    for (int i=0;i<COMMAND_COUNT;i++) assert(counts[i]==1);
    assert(shortcut_help_group_top(0,true)==0 && shortcut_help_group_top(1,true)==0);
    assert(shortcut_help_content_height(false)>shortcut_help_content_height(true));
    assert(shortcut_help_scroll(0,-100,40)==0 && shortcut_help_scroll(0,100,40)==40);
    assert(shortcut_help_scroll(100,0,40)==40 && shortcut_help_scroll(10,NAN,40)==10);
    assert(shortcut_help_scroll(NAN,5,40)==5 && shortcut_help_scroll(20,0,0)==0);
    return 0;
}
