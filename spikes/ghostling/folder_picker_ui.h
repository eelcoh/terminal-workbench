/* Native picker is a client child; it never touches the session service or PTYs. */
static Rectangle folder_picker_bounds(WorkspaceClient *client,Rectangle bounds)
{bool dual=client->dialog_kind==DIALOG_WORKSPACE_CREATE;return (Rectangle){bounds.x+320,bounds.y+(dual ? 116 : 64),76,dual ? 28 : 36};}
static void folder_picker_cancel(WorkspaceClient *client)
{
    if (!client->picker_busy) return;
    client->picker_discard=true;
    if (client->picker_pid>0) kill(-client->picker_pid,SIGTERM);
}
static void folder_picker_read(WorkspaceClient *client)
{
    while (client->picker_length<sizeof(client->picker_reply)-1) {
        ssize_t n=read(client->picker_fd,client->picker_reply+client->picker_length,sizeof(client->picker_reply)-1-client->picker_length);
        if (n>0) client->picker_length+=(size_t)n;
        else if (n<0 && errno==EINTR) continue;
        else break;
    }
    if (client->picker_length==sizeof(client->picker_reply)-1) {strcpy(client->dialog_error,"Folder-picker result was too long; enter it manually");folder_picker_cancel(client);}
}
static void folder_picker_poll(WorkspaceClient *client)
{
    if (!client->picker_busy) return;
    folder_picker_read(client);
    int status=0;pid_t done=waitpid(client->picker_pid,&status,WNOHANG);
    if (!done || (done<0 && errno==EINTR)) return;
    folder_picker_read(client);
    client->picker_reply[client->picker_length]=0;
    close(client->picker_fd);client->picker_busy=false;client->picker_pid=-1;
    if (client->picker_discard) return;
    const char *reply=client->picker_reply;
    if (done>0 && WIFEXITED(status) && WEXITSTATUS(status)==0 && !strcmp(reply,"CANCEL\n")) return;
    if (done>0 && WIFEXITED(status) && WEXITSTATUS(status)==0 && !strncmp(reply,"PICK\t",5)) {
        size_t length=strcspn(reply+5,"\r\n");
        if (length && length<sizeof(client->edit_directory) && reply[5]=='/' && !strchr(reply+5,'\t') && reply[5+length]=='\n' && !reply[6+length]) {
            memcpy(client->edit_directory,reply+5,length);client->edit_directory[length]=0;
            text_edit_init(&client->edit_fields[1].selection,client->edit_directory,false);
            client->edit_fields[1].scroll=0;client->edit_fields[1].blink=GetTime();
            client->dialog_focus=(UiFocus){.item=client->dialog_kind==DIALOG_WORKSPACE_CREATE ? 1 : 0,.visible=true};
            client->dialog_error[0]=0;return;
        }
    }
    const char *error=!strncmp(reply,"ERROR\t",6) ? reply+6 : "Could not choose a folder; enter it manually";
    snprintf(client->dialog_error,sizeof(client->dialog_error),"%.*s",(int)strcspn(error,"\r\n"),error);
}
static void folder_picker_start(WorkspaceClient *client)
{
    if (client->picker_busy || client->management_busy) return;
    const char *catalog=getenv("WORKBENCH_CATALOG_SCRIPT"),*project=getenv("WORKBENCH_PROJECT");
    if (!catalog || !project) {strcpy(client->dialog_error,"Folder browsing requires the workspace launcher");return;}
    char script[1200];const char *slash=strrchr(catalog,'/');
    if (!slash || (size_t)(slash-catalog)+18>=sizeof(script)) {strcpy(client->dialog_error,"Could not locate the folder picker");return;}
    snprintf(script,sizeof(script),"%.*s/folder_picker.py",(int)(slash-catalog),catalog);
    char *arguments[]={"python3",script,"--initial",client->edit_directory,"--cwd",(char *)project,NULL};
    int fds[2];if (pipe(fds)<0) {strcpy(client->dialog_error,"Could not start the folder picker");return;}
    int flags=fcntl(fds[0],F_GETFL);
    if (flags<0 || fcntl(fds[0],F_SETFL,flags|O_NONBLOCK)<0) {close(fds[0]);close(fds[1]);return;}
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    int error=posix_spawn_file_actions_init(&actions);bool actions_ready=!error,attributes_ready=false;
    if (!error) {error=posix_spawnattr_init(&attributes);attributes_ready=!error;}
    if (!error) error=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    if (!error) error=posix_spawnattr_setpgroup(&attributes,0);
    if (!error) error=posix_spawn_file_actions_adddup2(&actions,fds[1],STDOUT_FILENO);
    if (!error) error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    if (!error) error=posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    if (!error) error=posix_spawn_file_actions_addclose(&actions,fds[0]);
    if (!error) error=posix_spawn_file_actions_addclose(&actions,fds[1]);
    for (int w=0;!error && w<client->count;w++) for (int p=0;!error && p<client->workspaces[w].count;p++) {
        int fd=client->workspaces[w].panes[p]->fd;if (fd>=0) error=posix_spawn_file_actions_addclose(&actions,fd);
    }
    if (!error) error=posix_spawnp(&client->picker_pid,"python3",&actions,&attributes,arguments,environ);
    if (attributes_ready) posix_spawnattr_destroy(&attributes);
    if (actions_ready) posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (error) {close(fds[0]);strcpy(client->dialog_error,"Could not start the folder picker");return;}
    client->picker_fd=fds[0];client->picker_length=0;client->picker_busy=true;client->picker_discard=false;client->dialog_error[0]=0;
}
