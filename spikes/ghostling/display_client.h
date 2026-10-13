/* One window-level display probe, outside pane loops; no terminal reconnects. */
static void display_release_canvases(WorkspaceClient *client)
{
    for (int w=0;w<client->count;w++) for (int p=0;p<client->workspaces[w].count;p++) {
        WorkbenchPane *pane=client->workspaces[w].panes[p];
        if (pane->canvas.id) UnloadRenderTexture(pane->canvas);
        pane->canvas=(RenderTexture2D){0};pane->preview_epoch=0;
    }
    for (int p=0;p<WORKBENCH_PANES;p++) {
        if (client->navigation_tiles[p].canvas.id) UnloadRenderTexture(client->navigation_tiles[p].canvas);
        client->navigation_tiles[p]=(NavigationTile){0};
    }
    if (client->help_canvas.id) UnloadRenderTexture(client->help_canvas);
    client->help_canvas=(RenderTexture2D){0};client->help_dirty=true;client->help_cache_failed=false;
    if (client->overview_stage.id) UnloadRenderTexture(client->overview_stage);
    client->overview_stage=(RenderTexture2D){0};client->overview_epoch++;client->navigation_reset=true;
}
static float display_current_scale(WorkspaceClient *client)
{
    if (client->raster_override>0) return client->raster_override;
    int width=0,height=0;WorkbenchFramebufferSize(&width,&height);
    return display_raster_scale(GetScreenWidth(),GetScreenHeight(),width,height);
}
static void display_fit(WorkspaceClient *client,bool startup)
{
    DisplayWorkarea work=platform_workarea();
    if (!work.count) return;
    DisplayWorkarea previous=client->display_work;client->display_work=work;
    if (IsWindowFullscreen() || IsWindowState(FLAG_WINDOW_MAXIMIZED) || IsWindowState(FLAG_WINDOW_MINIMIZED)) return;
    Vector2 position=GetWindowPosition();
    DisplayRect original={(int)position.x,(int)position.y,GetScreenWidth(),GetScreenHeight()};
    DisplayRect fit=display_fit_window(original,work);
    if (!display_should_fit(previous,work,original,fit,startup)) return;
    if (fit.width!=original.width || fit.height!=original.height) SetWindowSize(fit.width,fit.height);
    if (fit.x!=original.x || fit.y!=original.y) SetWindowPosition(fit.x,fit.y);
}
static void display_tick(WorkspaceClient *client)
{
    double now=GetTime();
    bool periodic=now-client->display_checked>=.25;
    if (!periodic && !IsWindowResized()) return;
    if (periodic) {
        client->display_checked=now;display_fit(client,false);
        const char *probe=getenv("WORKBENCH_RENDER_SCALE_FILE");
        if (probe) {
            FILE *file=fopen(probe,"r");float scale=0;
            if (file) { if (fscanf(file,"%f",&scale)==1 && isfinite(scale) && scale>=1 && scale<=4) client->raster_override=scale;fclose(file); }
        }
    }
    float scale=display_current_scale(client);
    if (!scale || fabsf(scale-client->raster_scale)<.01f) return;
    if (!text_font_replace(client,client->text,scale)) {
        fprintf(stderr,"Could not update display fonts; retaining the previous atlas\n");return;
    }
    fprintf(stderr,"Display backing scale updated: %.3g; logical cells %dx%d retained\n",scale,client->cell_width,client->cell_height);
}
