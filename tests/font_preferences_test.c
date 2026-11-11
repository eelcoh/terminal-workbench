#include <assert.h>
#include "font_preferences.h"
int main(void) {
    assert(font_preferences_valid(font_preferences_default()));
    assert(font_preferences_family("Fira Code")==1 && font_preferences_family("jetbrains mono")==0);
    assert(font_preferences_family("absent")==-1);
    assert(!font_preferences_valid((FontPreferences){.size=9,.family=0}) && !font_preferences_valid((FontPreferences){.size=16,.family=TERMINAL_FONT_INSTALLED}));
    assert(font_preferences_valid((FontPreferences){.size=16,.family=TERMINAL_FONT_INSTALLED,.installed="Iosevka Term"}));
    assert(font_preferences_installed_name("SF Mono") && !font_preferences_installed_name("") && !font_preferences_installed_name("Mono # comment"));
    for (int i=10;i<=32;i++) {
        assert(font_preferences_step(i,1)==(i==32 ? 32 : i+1));
        assert(font_preferences_step(i,-1)==(i==10 ? 10 : i-1));
    }
    return 0;
}
