/* Original Workbench palettes and a bounded Ghostty color-file reader. */
#ifndef WORKBENCH_THEME_CORE_H
#define WORKBENCH_THEME_CORE_H
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t r, g, b; } ThemeRgb;
typedef struct {
    ThemeRgb background, foreground, cursor, selection_background, selection_foreground;
    ThemeRgb palette[256];
} Theme;
static ThemeRgb theme_rgb(unsigned int hex)
{
    return (ThemeRgb){(uint8_t)(hex >> 16), (uint8_t)(hex >> 8), (uint8_t)hex};
}
static bool theme_light(const Theme *theme)
{
    ThemeRgb b = theme->background;
    return b.r * 299 + b.g * 587 + b.b * 114 > 140000;
}
static Theme theme_preset(bool light)
{
    Theme theme = {0};
    theme.background = theme_rgb(light ? 0xf4f1eb : 0x15191f);
    theme.foreground = theme_rgb(light ? 0x303841 : 0xe1e7ef);
    theme.cursor = theme_rgb(light ? 0x366d82 : 0x9ac9dd);
    theme.selection_background = theme_rgb(light ? 0xc8dde4 : 0x354e60);
    theme.selection_foreground = theme_rgb(light ? 0x152e3a : 0xf4f8fc);
    const unsigned int dark[] = {0x27303c,0xe49b9b,0xa5c9ab,0xe3c58d,0x9bbbe2,0xc3acd9,0x96cbd0,0xdce3eb,
                                0x737f90,0xf1b1b1,0xb9dbba,0xf0d8a1,0xb6d2f2,0xdac6eb,0xb2e0e4,0xf4f7fb};
    const unsigned int bright[] = {0x303841,0xa93f4a,0x3c704d,0x8a621e,0x3b659c,0x7854a0,0x267583,0xd5d1c9,
                                  0x68747f,0xc65360,0x4c845d,0xa9792b,0x527ab0,0x906bb7,0x3a8c9b,0xffffff};
    for (int i = 0; i < 16; i++) theme.palette[i] = theme_rgb(light ? bright[i] : dark[i]);
    const uint8_t levels[] = {0,95,135,175,215,255};
    for (int i = 16; i < 232; i++) {
        int index = i - 16;
        theme.palette[i] = (ThemeRgb){levels[index / 36],levels[(index / 6) % 6],levels[index % 6]};
    }
    for (int i = 232; i < 256; i++) theme.palette[i] = (ThemeRgb){8+(i-232)*10,8+(i-232)*10,8+(i-232)*10};
    return theme;
}
static char *theme_trim(char *text)
{
    while (isspace((unsigned char)*text)) text++;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length-1])) text[--length] = '\0';
    return text;
}
static bool theme_hex(char *value, ThemeRgb *color)
{
    value = theme_trim(value);
    size_t length = strlen(value);
    if (length >= 2 && value[0] == '"' && value[length-1] == '"') {
        value[length-1] = '\0'; value++; length -= 2;
    }
    if (length && value[0] == '#') { value++; length--; }
    if (length != 6) return false;
    for (size_t i = 0; i < length; i++) if (!isxdigit((unsigned char)value[i])) return false;
    *color = theme_rgb((unsigned int)strtoul(value, NULL, 16));
    return true;
}
/* Parsing commits only a complete valid color file; unsupported keys are counted
 * and never executed (including config-file, commands and keybindings). */
static char *theme_text_line(const char **text, char *line, size_t capacity)
{
    if (!**text) return NULL;
    size_t length=0;
    while (length<capacity-1 && (*text)[length]) {
        line[length]=(*text)[length]; length++;
        if (line[length-1]=='\n') break;
    }
    line[length]='\0'; *text+=length; return line;
}
static bool theme_read_source(FILE *file, const char *text, const Theme *base, Theme *result, int *ignored, char *error, size_t capacity)
{
    Theme next = *base;
    char line[1024]; int number = 0, recognized = 0;
    *ignored = 0;
    while (file ? fgets(line,sizeof(line),file) : theme_text_line(&text,line,sizeof(line))) {
        number++;
        if (!strchr(line,'\n') && (file ? !feof(file) : *text!='\0')) goto invalid;
        char *key = theme_trim(line);
        if (!*key || *key == '#') continue;
        char *equals = strchr(key, '=');
        if (!equals) goto invalid;
        *equals++ = '\0'; key = theme_trim(key);
        char *value = theme_trim(equals);
        ThemeRgb *color = NULL;
        if (!strcmp(key,"background")) color = &next.background;
        else if (!strcmp(key,"foreground")) color = &next.foreground;
        else if (!strcmp(key,"cursor-color")) color = &next.cursor;
        else if (!strcmp(key,"selection-background")) color = &next.selection_background;
        else if (!strcmp(key,"selection-foreground")) color = &next.selection_foreground;
        else if (!strcmp(key,"palette")) {
            char *separator = strchr(value,'=');
            if (!separator) goto invalid;
            *separator++ = '\0'; value = theme_trim(value);
            if (!*value) goto invalid;
            for (char *p=value; *p; p++) if (!isdigit((unsigned char)*p)) goto invalid;
            unsigned long index = strtoul(value,NULL,10);
            if (index > 255 || strlen(value) > 3) goto invalid;
            color = &next.palette[index]; value = separator;
        } else { (*ignored)++; continue; }
        if (!theme_hex(value,color)) goto invalid;
        recognized++;
    }
    if (file && ferror(file)) { snprintf(error,capacity,"Could not read theme"); return false; }
    if (!recognized) { snprintf(error,capacity,"Theme contains no supported colors"); return false; }
    *result = next; error[0] = '\0'; return true;
invalid:
    snprintf(error,capacity,"Invalid theme color at line %d",number); return false;
}
static bool theme_read(FILE *file, const Theme *base, Theme *result, int *ignored, char *error, size_t capacity)
{
    return theme_read_source(file,NULL,base,result,ignored,error,capacity);
}
static bool theme_read_text(const char *text, const Theme *base, Theme *result, int *ignored, char *error, size_t capacity)
{
    return theme_read_source(NULL,text,base,result,ignored,error,capacity);
}
#endif
