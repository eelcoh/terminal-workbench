#include <assert.h>
#include "appearance_state.h"
static bool read(const char *text, AppearanceState *state, char *error)
{
    FILE *file=tmpfile(); assert(file); fputs(text,file); rewind(file);
    bool valid=appearance_read(file,state,error,128); fclose(file); return valid;
}
int main(void)
{
    Theme night=theme_preset(false),dawn=theme_preset(true); assert(theme_light(&dawn));
    AppearanceState state=appearance_default(night);
    assert(!state.follow && !memcmp(appearance_chrome(&state,"project"),&night,sizeof(Theme)));
    assert(appearance_set(state.workspaces,&state.workspace_count,8,"project",&dawn));
    assert(!memcmp(appearance_terminal(&state,"project","shell"),&dawn,sizeof(Theme)));
    assert(!memcmp(appearance_chrome(&state,"project"),&night,sizeof(Theme)));
    assert(appearance_set(state.terminals,&state.terminal_count,64,"shell",&night));
    state.follow=true;
    assert(!memcmp(appearance_chrome(&state,"project"),&dawn,sizeof(Theme)));
    assert(!memcmp(appearance_terminal(&state,"project","shell"),&night,sizeof(Theme)));
    /* Moving changes inheritance; an explicit terminal palette follows its ID. */
    assert(!memcmp(appearance_terminal(&state,"other","shell"),&night,sizeof(Theme)));
    assert(!memcmp(appearance_terminal(&state,"other","unscoped"),&night,sizeof(Theme)));
    assert(appearance_set(state.terminals,&state.terminal_count,64,"shell",NULL));
    assert(!memcmp(appearance_terminal(&state,"project","shell"),&dawn,sizeof(Theme)));
    assert(appearance_set(state.terminals,&state.terminal_count,64,"shell",&night));
    FILE *file=tmpfile(); assert(file); assert(appearance_write(file,&state)); rewind(file);
    AppearanceState restored=appearance_default(dawn); char error[128];
    assert(appearance_read(file,&restored,error,sizeof(error))); fclose(file);
    assert(restored.follow && restored.workspace_count==1 && restored.terminal_count==1);
    assert(!memcmp(appearance_terminal(&restored,"project","shell"),&night,sizeof(Theme)));
    assert(!memcmp(appearance_workspace(&restored,"project"),&dawn,sizeof(Theme)));
    AppearanceState before=restored;
    const char *bad[]={"workbench-appearance-v2\n", "workbench-appearance-v1\nfollow\t2\n", "workbench-appearance-v1\nfollow\t0\n@application\nbackground=#111111\n", "workbench-appearance-v1\nfollow\t0\n@application\nbackground=#111111\n@default\nbackground=bad\n", "workbench-appearance-v1\nfollow\t0\n@application\nbackground=#111111\n@default\nbackground=#222222\n@workspace\tproject\nbackground=#333333\n@workspace\tproject\nbackground=#444444\n"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++) { assert(!read(bad[i],&restored,error)); assert(!memcmp(&before,&restored,sizeof(before))); }
    AppearanceBinding bindings[1]={0}; int count=0;
    assert(!appearance_set(bindings,&count,1,"bad\tid",&night));
    assert(appearance_set(bindings,&count,1,"one",&night));
    assert(!appearance_set(bindings,&count,1,"two",&night) && count==1);
    assert(appearance_set(bindings,&count,1,"one",NULL) && count==0);
    /* The legacy Ghostty color reader still supplies migration defaults. */
    file=tmpfile(); assert(file); fputs("background=#f4f1eb\n",file); rewind(file); int ignored;
    assert(theme_read(file,&night,&dawn,&ignored,error,sizeof(error))); fclose(file);
    puts("PASS: stable/follow chrome, workspace/default inheritance, terminal overrides and identity moves, scoped roundtrip, limits and transactional corruption isolation");
}
