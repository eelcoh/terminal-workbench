/* Catalog ownership changes must reuse live panes without reconnecting. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#define WORKBENCH_MAX_WORKSPACES 8
#define WORKBENCH_PANES 8
typedef struct {
    char id[41], name[64], socket_path[108];
    int fd;
    struct { int agent_fd; bool replaying; } transport;
    int retained_parser_marker;
} WorkbenchPane;
typedef struct {
    char id[41], name[64];
    WorkbenchPane *panes[8];
    int count, focused, first;
} WorkbenchWorkspace;
typedef struct {
    WorkbenchWorkspace workspaces[8];
    int count, active, sidebar_first;
    char catalog_path[108];
} WorkspaceClient;
static int initializations, frees;
static bool pane_init(WorkspaceClient *client, WorkbenchPane *pane) {
    (void)client; (void)pane; initializations++; return true;
}
static void pane_free(WorkbenchPane *pane) { (void)pane; frees++; }
#include "catalog_view.h"
static void terminal(CatalogSnapshot *s, int w, int p, WorkbenchPane *pane) {
    CatalogTerminal *t = &s->workspaces[w].terminals[p];
    strcpy(t->id,pane->id); strcpy(t->name,pane->name); strcpy(t->path,pane->socket_path);
    s->workspaces[w].count++;
}
int main(void) {
    WorkbenchPane one = {.id="one",.name="One",.socket_path="/tmp/one.sock",.fd=77,.retained_parser_marker=123},
        two = {.id="two",.name="Two",.socket_path="/tmp/two.sock",.fd=88,.retained_parser_marker=456},
        three = {.id="three",.name="Three",.socket_path="/tmp/three.sock",.fd=99};
    WorkspaceClient client = {.count=2,.active=0};
    strcpy(client.workspaces[0].id,"a"); strcpy(client.workspaces[1].id,"b");
    client.workspaces[0].count=2; client.workspaces[0].focused=0;
    client.workspaces[0].panes[0]=&one; client.workspaces[0].panes[1]=&two;
    client.workspaces[1].count=1; client.workspaces[1].panes[0]=&three;
    CatalogSnapshot s = {.count=2};
    strcpy(s.workspaces[0].id,"a"); strcpy(s.workspaces[0].name,"Source");
    strcpy(s.workspaces[1].id,"b"); strcpy(s.workspaces[1].name,"Target");
    terminal(&s,0,0,&two); terminal(&s,1,0,&one); terminal(&s,1,1,&three);
    assert(catalog_apply(&client,&s,true));
    assert(client.workspaces[0].panes[0]==&two && client.workspaces[0].focused==0);
    assert(client.workspaces[1].panes[0]==&one && client.workspaces[1].focused==1);
    assert(one.fd==77 && one.retained_parser_marker==123 && two.fd==88);
    assert(initializations==0 && frees==0 && client.active==0);
    s.workspaces[0].count=0; s.workspaces[1].count=0;
    terminal(&s,1,0,&two); terminal(&s,1,1,&one); terminal(&s,1,2,&three);
    assert(catalog_apply(&client,&s,true));
    assert(client.workspaces[0].count==0 && client.workspaces[0].panes[0]==NULL);
    assert(client.workspaces[1].panes[1]==&one && client.workspaces[1].focused==2);
    assert(initializations==0 && frees==0);
    /* Exercise the normal reader/reload wrappers too, including an empty source. */
    char path[108]; snprintf(path,sizeof(path),"%s/view.tsv",getenv("WB_TEST_DIRECTORY"));
    FILE *f=fopen(path,"w"); assert(f);
    fprintf(f,"workbench-catalog-v3\nW\ta\tSource\nW\tb\tTarget\nT\tb\ttwo\tTwo\t/tmp/two.sock\nT\tb\tone\tOne\t/tmp/one.sock\nT\tb\tthree\tThree\t/tmp/three.sock\n"); fclose(f);
    strcpy(client.catalog_path,path);
    assert(workspace_reload(&client));
    assert(workspace_catalog(&client,path));
    assert(initializations==0 && frees==0);
    return 0;
}
