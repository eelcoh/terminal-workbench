#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <sys/stat.h>
#include "window_state.h"
int main(void)
{
    char path[1200]; snprintf(path,sizeof(path),"%s/window.view",getenv("WB_TEST_DIRECTORY"));
    WindowState original={.width=1360,.height=820,.divider=.375f,.vertical=2.25f,.overview=true,.list=true,.context=true,.count=2};
    strcpy(original.active,"-"); strcpy(original.sidebar,"empty"); strcpy(original.selected,"pane-7");
    strcpy(original.query,"two words \t café");
    original.workspaces[0]=(WindowWorkspaceState){.id="project",.focused="pane-7",.first="pane-6",.horizontal=3.125f};
    original.workspaces[1]=(WindowWorkspaceState){.id="empty"};
    assert(window_state_write(&original,path));
    WindowState read={0}; assert(window_state_read(&read,path));
    assert(!memcmp(&original,&read,sizeof(read)));
    struct stat metadata; assert(!stat(path,&metadata)); assert((metadata.st_mode&0777)==0600);
    const char *bad[]={"workbench-window-v2\n", "workbench-window-v1\nV 1360 820 nan project empty 1 0 0 0 1 pane - 1\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 1 0 0 0 inf pane - 1\n",
        "workbench-window-v1\nV 999999 820 .5 project empty 1 0 0 0 1 pane - 1\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 2 0 0 0 1 pane - 1\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 1 0 0 0 1 pane ff00 1\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 1 0 0 0 1 pane xyz 1\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 1 0 0 0 1 pane - 1\nW project pane pane 1\nW project pane pane 0\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 1 0 0 0 1 pane - 1\nW project pane pane -1\n",
        "workbench-window-v1\nV 1360 820 .5 project empty 1 0 0 0 1 pane -"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        FILE *file=fopen(path,"w"); assert(file); fputs(bad[i],file); fclose(file);
        read=original; assert(!window_state_read(&read,path)); assert(!memcmp(&read,&original,sizeof(read)));
    }
    assert(!window_state_write(&original,"/missing-workbench-dir/window.view"));
    unlink(path); puts("PASS: window state roundtrip, private atomic files, Unicode query, invalid/truncated/versioned state rejection");
}
