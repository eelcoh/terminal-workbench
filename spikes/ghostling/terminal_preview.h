/* Fixed-size cached previews use the retained viewport at its existing PTY
 * geometry. Camera scaling changes pixels only, never the terminal screen. */
static void pane_preview_refresh(WorkspaceClient *client, WorkbenchPane *pane)
{
    const int width = 384, height = 240;
    bool created = !pane->preview.id;
    if (created) {
        pane->preview = LoadRenderTexture(width, height);
        if (!pane->preview.id) return;
        SetTextureFilter(pane->preview.texture, TEXTURE_FILTER_BILINEAR);
    }
    double now = GetTime();
    if (created || now - pane->preview_updated >= 0.1) {
        GhosttyTerminalScrollbar scrollbar = {0};
        ghostty_terminal_get(pane->terminal, GHOSTTY_TERMINAL_DATA_SCROLLBAR, &scrollbar);
        if (created || pane->preview_epoch != client->overview_epoch
            || pane->preview_revision != pane->transport.output_revision
            || pane->preview_cols != pane->transport.cols || pane->preview_rows != pane->transport.rows
            || pane->preview_scroll != scrollbar.offset) {
            int surface_w = pane->transport.cols * client->cell_width + 8;
            int surface_h = pane->transport.rows * client->cell_height + 8;
            float zoom = (float)width / surface_w;
            if ((float)height / surface_h < zoom) zoom = (float)height / surface_h;
            pane_bind(pane);
            client_surface = (Rectangle){0, 0, surface_w, surface_h + WORKBENCH_HEADER_HEIGHT};
            ghostty_render_state_update(pane->render, pane->terminal);
            GhosttyRenderStateColors colors = GHOSTTY_INIT_SIZED(GhosttyRenderStateColors);
            ghostty_render_state_colors_get(pane->render, &colors);
            BeginTextureMode(pane->preview);
            ClearBackground((Color){colors.background.r, colors.background.g, colors.background.b, 255});
            Camera2D camera = { .target = {0, WORKBENCH_HEADER_HEIGHT}, .offset = {
                (width - surface_w * zoom) / 2, (height - surface_h * zoom) / 2}, .zoom = zoom };
            BeginMode2D(camera);
            render_terminal(pane->render, pane->rows, pane->cells, client->font,
                client->cell_width, client->cell_height, client->text.size, 4, &scrollbar, pane->terminal, pane->images);
            EndMode2D();
            EndTextureMode();
            pane_unbind();
            pane->preview_revision = pane->transport.output_revision;
            pane->preview_epoch = client->overview_epoch;
            pane->preview_cols = pane->transport.cols; pane->preview_rows = pane->transport.rows;
            pane->preview_scroll = scrollbar.offset;
        }
        pane->preview_updated = now;
    }
}

static void pane_preview_draw(WorkspaceClient *client, WorkbenchPane *pane, Rectangle target)
{
    if (!pane->preview.id || target.width < 8 || target.height < 8) return;
    const int width = 384, height = 240;
    float scale = target.width / width;
    if (target.height / height < scale) scale = target.height / height;
    Rectangle fitted = {target.x + (target.width - width * scale) / 2,
        target.y + (target.height - height * scale) / 2, width * scale, height * scale};
    DrawTexturePro(pane->preview.texture, (Rectangle){0, 0, width, -height}, fitted, (Vector2){0, 0}, 0, WHITE);
}
