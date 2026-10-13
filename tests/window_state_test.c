#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <sys/stat.h>
#include "window_state.h"
int main(void)
{
    char path[1200]; snprintf(path,sizeof(path),"%s/window.view",getenv("WB_TEST_DIRECTORY"));
    WindowState original={.width=1360,.height=820,.divider=.375f,.vertical=2.25f,.overview=true,.list=true,.context=true,.count=2,.chrome_pinned=true,.chrome_agents=true,.chrome_waiting=true,.font_size=22,.font_family=2};
    strcpy(original.active,"-"); strcpy(original.sidebar,"empty"); strcpy(original.selected,"pane-7");
    strcpy(original.query,"two words \t café");
    original.workspaces[0]=(WindowWorkspaceState){.id="project",.focused="pane-7",.first="pane-6",.horizontal=3.125f};
    original.workspaces[0].scroll=2.5f;original.workspaces[0].terminals=1;
    original.workspaces[0].widths[0]=(WindowTerminalState){.id="pane-7",.preset=1,.full=true};
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
    FILE *legacy=fopen(path,"w");assert(legacy);
    fputs("workbench-window-v1\nV 1360 820 .25 project . 0 0 0 0 0 . - 0\nW project pane-7 pane-6 0\n",legacy);fclose(legacy);
    assert(window_state_read(&read,path) && read.workspaces[0].terminals==0 && read.workspaces[0].scroll==0);
    assert(!read.chrome_pinned && !read.chrome_agents && !read.chrome_waiting);
    legacy=fopen(path,"w");assert(legacy);
    fputs("workbench-window-v2\nV 1360 820 .5 project . 0 0 0 0 0 . - 0\nW project pane-7 pane-7 0 1\nT pane-7 2 1\n",legacy);fclose(legacy);
    assert(window_state_read(&read,path) && read.workspaces[0].terminals==1 && read.workspaces[0].widths[0].full && !read.chrome_pinned);
    const char *chrome_bad[]={"C 2 0 0\n", "C 0 -1 0\n", "C 0 0 3\n", "C 1 1\n"};
    for (size_t i=0;i<sizeof(chrome_bad)/sizeof(*chrome_bad);i++) {
        FILE *file=fopen(path,"w");assert(file);
        fputs("workbench-window-v3\nV 1360 820 .5 project . 0 0 0 0 0 . - 0\n",file);fputs(chrome_bad[i],file);fclose(file);
        read=original;assert(!window_state_read(&read,path) && !memcmp(&read,&original,sizeof(read)));
    }
    legacy=fopen(path,"w");assert(legacy);
    fputs("workbench-window-v3\nV 1360 820 .5 project . 0 0 0 0 0 . - 0\nC 1 0 0\n",legacy);fclose(legacy);
    assert(window_state_read(&read,path) && !read.font_size && !read.font_family && read.chrome_pinned);
    const char *font_bad[]={"F 9 0\n","F 33 0\n","F 16 -1\n","F 16 3\n","F 16\n","F 16 1 extra\n"};
    for (size_t i=0;i<sizeof(font_bad)/sizeof(*font_bad);i++) {
        FILE *file=fopen(path,"w");assert(file);
        fputs("workbench-window-v4\nV 1360 820 .5 project . 0 0 0 0 0 . - 0\nC 0 0 0\n",file);fputs(font_bad[i],file);fclose(file);
        read=original;assert(!window_state_read(&read,path) && !memcmp(&read,&original,sizeof(read)));
    }
    const char *invalid_widths[]={"T pane-7 3 0\n", "T pane-7 0 2\n", "T pane-7 0 0\nT pane-7 1 0\n"};
    for (size_t i=0;i<sizeof(invalid_widths)/sizeof(*invalid_widths);i++) {
        FILE *file=fopen(path,"w");assert(file);
        fputs("workbench-window-v2\nV 1360 820 .5 project . 0 0 0 0 0 . - 0\nW project pane-7 pane-7 0 0\n",file);
        fputs(invalid_widths[i],file);fclose(file);
        read=original;assert(!window_state_read(&read,path) && !memcmp(&read,&original,sizeof(read)));
    }
    assert(!window_state_write(&original,"/missing-workbench-dir/window.view"));
    unlink(path); puts("PASS: window state roundtrip, private atomic files, Unicode query, invalid/truncated/versioned state rejection");
}
