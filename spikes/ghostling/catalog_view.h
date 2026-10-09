/* Read a complete metadata projection before touching live pane identities. */
typedef struct {
    char id[41], name[64], path[sizeof(((struct sockaddr_un *)0)->sun_path)];
} CatalogTerminal;
typedef struct {
    char id[41], name[64];
    CatalogTerminal terminals[WORKBENCH_PANES];
    int count;
} CatalogWorkspace;
typedef struct {
    CatalogWorkspace workspaces[WORKBENCH_MAX_WORKSPACES];
    int count;
} CatalogSnapshot;

static bool catalog_field(char *destination, size_t capacity, const char *source)
{
    size_t length = strlen(source);
    if (!length || length >= capacity) return false;
    for (size_t i = 0; i < length; i++)
        if ((unsigned char)source[i] < 32 || source[i] == 127) return false;
    memcpy(destination, source, length + 1);
    return true;
}

static bool catalog_read(CatalogSnapshot *snapshot, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) { perror("workspace catalog"); return false; }
    char line[512];
    bool valid = true, version2 = false, version3 = false;
    int line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        if (!strchr(line, '\n')) { valid = false; break; }
        line[strcspn(line, "\r\n")] = '\0';
        if (line_number++ == 0 && strcmp(line, "workbench-catalog-v2") == 0) {
            version2 = true; continue;
        }
        if (line_number == 1 && strcmp(line, "workbench-catalog-v3") == 0) {
            version3 = true; continue;
        }
        char *fields[5] = {line};
        int field_count = 1;
        for (char *next = strchr(line, '\t'); next; next = strchr(next, '\t')) {
            *next++ = '\0';
            if (field_count == 5) { valid = false; break; }
            fields[field_count++] = next;
        }
        if (version3 && valid) {
            if (strcmp(fields[0], "W") == 0 && field_count == 3) {
                if (snapshot->count == WORKBENCH_MAX_WORKSPACES) { valid = false; break; }
                for (int w = 0; w < snapshot->count; w++)
                    if (strcmp(snapshot->workspaces[w].id, fields[1]) == 0) valid = false;
                CatalogWorkspace *workspace = &snapshot->workspaces[snapshot->count++];
                if (!catalog_field(workspace->id, sizeof(workspace->id), fields[1])
                    || !catalog_field(workspace->name, sizeof(workspace->name), fields[2])) valid = false;
            } else if (strcmp(fields[0], "T") == 0 && field_count == 5 && snapshot->count > 0) {
                CatalogWorkspace *workspace = &snapshot->workspaces[snapshot->count - 1];
                if (strcmp(workspace->id, fields[1]) || workspace->count == WORKBENCH_PANES) { valid = false; break; }
                CatalogTerminal *terminal = &workspace->terminals[workspace->count];
                if (!catalog_field(terminal->id, sizeof(terminal->id), fields[2])
                    || !catalog_field(terminal->name, sizeof(terminal->name), fields[3])
                    || !catalog_field(terminal->path, sizeof(terminal->path), fields[4])) valid = false;
                for (int w = 0; w < snapshot->count; w++)
                    for (int t = 0; t < snapshot->workspaces[w].count; t++)
                        if (!strcmp(snapshot->workspaces[w].terminals[t].id, terminal->id)
                            || !strcmp(snapshot->workspaces[w].terminals[t].path, terminal->path)) valid = false;
                workspace->count++;
            } else valid = false;
            if (!valid) break;
            continue;
        }
        if (!valid || field_count != (version2 ? 5 : 3)) { valid = false; break; }
        int index = snapshot->count;
        if (version2)
            for (int w = 0; w < snapshot->count; w++)
                if (strcmp(snapshot->workspaces[w].id, fields[0]) == 0) index = w;
        if (index == WORKBENCH_MAX_WORKSPACES) { valid = false; break; }
        CatalogWorkspace *workspace = &snapshot->workspaces[index];
        char legacy_id[41];
        snprintf(legacy_id, sizeof(legacy_id), "workspace-%d", index);
        const char *workspace_id = version2 ? fields[0] : legacy_id;
        const char *workspace_name = version2 ? fields[1] : fields[0];
        if (index == snapshot->count) {
            if (!catalog_field(workspace->id, sizeof(workspace->id), workspace_id)
                || !catalog_field(workspace->name, sizeof(workspace->name), workspace_name)) { valid = false; break; }
            snapshot->count++;
        } else if (strcmp(workspace->name, workspace_name) != 0) { valid = false; break; }
        int number = version2 ? 1 : 2;
        for (int p = 0; p < number; p++) {
            if (workspace->count == WORKBENCH_PANES) { valid = false; break; }
            CatalogTerminal *terminal = &workspace->terminals[workspace->count];
            char id[41], name[64];
            snprintf(id, sizeof(id), "terminal-%d-%d", index, p);
            snprintf(name, sizeof(name), "Shell %d", p + 1);
            if (!catalog_field(terminal->id, sizeof(terminal->id), version2 ? fields[2] : id)
                || !catalog_field(terminal->name, sizeof(terminal->name), version2 ? fields[3] : name)
                || !catalog_field(terminal->path, sizeof(terminal->path), version2 ? fields[4] : fields[p + 1])) { valid = false; break; }
            for (int w = 0; w < snapshot->count; w++) {
                for (int t = 0; t < snapshot->workspaces[w].count; t++) {
                    CatalogTerminal *other = &snapshot->workspaces[w].terminals[t];
                    if (strcmp(other->id, terminal->id) == 0 || strcmp(other->path, terminal->path) == 0) valid = false;
                }
            }
            if (!valid) break;
            workspace->count++;
        }
        if (!valid) break;
    }
    if (ferror(file)) valid = false;
    fclose(file);
    return valid && (snapshot->count > 0 || version3);
}

static bool catalog_apply(WorkspaceClient *client, CatalogSnapshot *snapshot, bool initialize)
{
    WorkbenchWorkspace updated[WORKBENCH_MAX_WORKSPACES] = {0};
    char active_id[41];
    strcpy(active_id, client->workspaces[client->active].id);
    bool valid = true;
    for (int w = 0; w < snapshot->count && valid; w++) {
        CatalogWorkspace *saved = &snapshot->workspaces[w];
        WorkbenchWorkspace *next = &updated[w], *old = NULL;
        for (int i = 0; i < client->count; i++)
            if (!strcmp(client->workspaces[i].id, saved->id)) old = &client->workspaces[i];
        strcpy(next->id, saved->id); strcpy(next->name, saved->name);
        char focused_id[41] = {0};
        if (old && old->count) strcpy(focused_id, old->panes[old->focused]->id);
        for (int p = 0; p < saved->count; p++) {
            CatalogTerminal *terminal = &saved->terminals[p];
            WorkbenchPane *pane = NULL;
            /* A terminal may have moved from another workspace. Reuse its heap
             * identity, connection, parser, selection and cached textures. */
            for (int source = 0; source < client->count; source++)
                for (int i = 0; i < client->workspaces[source].count; i++)
                    if (!strcmp(client->workspaces[source].panes[i]->id, terminal->id))
                        pane = client->workspaces[source].panes[i];
            bool fresh = !pane;
            if (fresh) pane = calloc(1, sizeof(*pane));
            if (!pane) { valid = false; break; }
            next->panes[next->count++] = pane;
            if (!fresh && strcmp(pane->socket_path, terminal->path)) { valid = false; break; }
            strcpy(pane->id, terminal->id); strcpy(pane->name, terminal->name);
            strcpy(pane->socket_path, terminal->path);
            if (fresh) {
                pane->fd = pane->transport.agent_fd = -1; pane->transport.replaying = true;
                if (initialize && !pane_init(client, pane)) { valid = false; break; }
            }
            if (!strcmp(focused_id, pane->id)) next->focused = p;
        }
        if (old && old->count && next->count && focused_id[0]) {
            bool retained_focus = false;
            for (int p = 0; p < next->count; p++) retained_focus |= !strcmp(next->panes[p]->id, focused_id);
            if (!retained_focus) next->focused = old->focused < next->count ? old->focused : next->count - 1;
        }
        if (next->focused > 1) next->first = next->focused - 1;
    }
    /* Free only resources not retained in the chosen state. Heap addresses never
     * change, including userdata referenced by terminal effects callbacks. */
    WorkbenchWorkspace *discard = valid ? client->workspaces : updated;
    WorkbenchWorkspace *keep = valid ? updated : client->workspaces;
    for (int w = 0; w < WORKBENCH_MAX_WORKSPACES; w++)
        for (int p = 0; p < discard[w].count; p++) {
            WorkbenchPane *pane = discard[w].panes[p];
            bool retained = false;
            for (int k = 0; k < WORKBENCH_MAX_WORKSPACES; k++)
                for (int t = 0; t < keep[k].count; t++) retained |= keep[k].panes[t] == pane;
            if (!retained) pane_free(pane);
        }
    if (!valid) return false;
    memcpy(client->workspaces, updated, sizeof(updated));
    client->count = snapshot->count;
    if (client->active >= client->count) client->active = client->count > 0 ? client->count - 1 : 0;
    for (int w = 0; w < client->count; w++)
        if (!strcmp(client->workspaces[w].id, active_id)) client->active = w;
    if (client->sidebar_first >= client->count) client->sidebar_first = client->count > 0 ? client->count - 1 : 0;
    return true;
}

static bool workspace_catalog(WorkspaceClient *client, const char *path)
{
    CatalogSnapshot snapshot = {0};
    if (!catalog_read(&snapshot, path) || !catalog_apply(client, &snapshot, false)) {
        fprintf(stderr, "invalid workspace catalog\n"); return false;
    }
    return true;
}

static bool workspace_reload(WorkspaceClient *client)
{
    CatalogSnapshot snapshot = {0};
    return catalog_read(&snapshot, client->catalog_path) && catalog_apply(client, &snapshot, true);
}
