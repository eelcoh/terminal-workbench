#include <assert.h>
#include "theme_catalog.h"
static bool read_text(const char *text, Theme *result, int *ignored, char *error)
{
    FILE *file=tmpfile(); assert(file);
    fputs(text,file); rewind(file);
    Theme base=theme_preset(false);
    bool valid=theme_read(file,&base,result,ignored,error,128);
    fclose(file); return valid;
}
int main(void)
{
    Theme night=theme_preset(false), dawn=theme_preset(true);
    assert(!theme_light(&night) && theme_light(&dawn));
    assert(night.palette[16].r==0 && night.palette[231].r==255 && night.palette[255].g==238);
    Theme parsed=night; int ignored; char error[128];
    assert(read_text("# Ghostty color syntax\nbackground = \"#F1F2F3\"\nforeground = 102030\npalette = 0=#123456\npalette=255=abcDEF\ncursor-color=#abcdef\nselection-background=#334455\nselection-foreground=#ddeeff\nfont-family = Example\nconfig-file = ignored\n",&parsed,&ignored,error));
    assert(ignored==2 && theme_light(&parsed));
    assert(parsed.palette[0].r==0x12 && parsed.palette[255].b==0xef && parsed.palette[42].g==night.palette[42].g);
    assert(parsed.selection_foreground.r==0xdd && parsed.cursor.g==0xcd);
    Theme before=parsed;
    const char *invalid[]={"background=#123\n","palette=256=#ffffff\n","palette=-1=#123456\n","palette=99999999999999999999999999=#123456\n","foreground=#12345z\n","background=#fff000\ncursor-color=bad\n","config-file=elsewhere\n","malformed\n"};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        assert(!read_text(invalid[i],&parsed,&ignored,error));
        assert(!memcmp(&before,&parsed,sizeof(Theme)) && error[0]);
    }
    assert(read_text("background=#112233",&parsed,&ignored,error));
    assert(parsed.background.b==0x33);
    Theme catalog[THEME_CATALOG_COUNT]; int skipped[THEME_CATALOG_COUNT];
    assert(theme_catalog_load(catalog,skipped,error,sizeof(error)));
    assert(THEME_CATALOG_COUNT==7 && !strcmp(theme_catalog_name(2),"Dracula"));
    assert(catalog[2].background.r==0x28 && catalog[3].background.b==0x2e);
    assert(theme_light(&catalog[6]) && !theme_light(&catalog[3]) && skipped[3]==2);
    assert(theme_catalog_matches(theme_catalog_name(3),&catalog[3],"MOCHA",1));
    assert(!theme_catalog_matches(theme_catalog_name(3),&catalog[3],"mocha",2));
    assert(theme_catalog_matches(theme_catalog_name(6),&catalog[6],"latte",2));
    parsed=before;
    assert(!theme_read_text("background=#123456\ncursor-color=bad",&night,&parsed,&ignored,error,sizeof(error)));
    assert(!memcmp(&parsed,&before,sizeof(Theme)));
    char oversized[1100]; memset(oversized,'x',sizeof(oversized)); oversized[1099]='\0';
    assert(!theme_read_text(oversized,&night,&parsed,&ignored,error,sizeof(error)));
    assert(theme_read_text("background=#112233",&night,&parsed,&ignored,error,sizeof(error)) && parsed.background.b==0x33);
    puts("PASS: dark/light presets, Ghostty color syntax, palette bounds, ignored non-color settings, partial defaults and transactional parse failures");
    return 0;
}
