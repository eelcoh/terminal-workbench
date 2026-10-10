/* Decode into a new VT, then bind client-only effects and appearance before
 * any live bytes arrive. Snapshot decoding never emits historical replies. */
static bool remote_restore_terminal(GhosttyTerminal *terminal, const uint8_t *bytes, size_t length, void *userdata)
{
    GhosttySnapshotDecoder decoder = NULL;
    GhosttyTerminal restored = NULL;
    size_t continuation = 4096, consumed = 0;
    if (ghostty_snapshot_decoder_new_buf(NULL,&decoder,bytes,length) != GHOSTTY_SUCCESS) return false;
    GhosttyResult result = ghostty_snapshot_decoder_set(decoder,GHOSTTY_SNAPSHOT_DECODER_OPT_MAX_CONTINUATION_BYTES,&continuation);
    if (result == GHOSTTY_SUCCESS) result = ghostty_snapshot_decoder_decode(decoder,&restored);
    if (result == GHOSTTY_SUCCESS) result = ghostty_snapshot_decoder_get(decoder,GHOSTTY_SNAPSHOT_DECODER_DATA_SOURCE_OFFSET,&consumed);
    ghostty_snapshot_decoder_free(decoder);
    if (result != GHOSTTY_SUCCESS || consumed != length) {
        if (restored) ghostty_terminal_free(restored);
        return false;
    }
    EffectsContext *effects = userdata;
    ghostty_terminal_get(restored,GHOSTTY_TERMINAL_DATA_COLS,&effects->cols);
    ghostty_terminal_get(restored,GHOSTTY_TERMINAL_DATA_ROWS,&effects->rows);
    ui_apply_terminal(restored,effects->theme ? effects->theme : &current_theme);
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_USERDATA,effects);
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_WRITE_PTY,(const void *)effect_write_pty);
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_SIZE,(const void *)effect_size);
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_DEVICE_ATTRIBUTES,(const void *)effect_device_attributes);
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_XTVERSION,(const void *)effect_xtversion);
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_COLOR_SCHEME,(const void *)effect_color_scheme);
    uint64_t images = (effects->theme ? 16 : 64) * 1024 * 1024;
    ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_STORAGE_LIMIT,&images);
    if (!effects->theme) {
        bool medium_enabled = true;
        ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_TITLE_CHANGED,(const void *)effect_title_changed);
        ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_FILE,&medium_enabled);
        ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_TEMP_FILE,&medium_enabled);
        ghostty_terminal_set(restored,GHOSTTY_TERMINAL_OPT_KITTY_IMAGE_MEDIUM_SHARED_MEM,&medium_enabled);
    }
    if (ghostty_terminal_resize(restored,effects->cols,effects->rows,effects->cell_width,effects->cell_height) != GHOSTTY_SUCCESS) {
        ghostty_terminal_free(restored); return false;
    }
    ghostty_terminal_free(*terminal);
    *terminal = restored;
    fprintf(stderr,"session attachment: restored terminal snapshot (%zu bytes)\n",length);
    return true;
}
