#include <assert.h>
#include "chrome_menu_layout.h"
int main(void) {
    for (int kind=1;kind<=3;kind++) for (int count=1;count<=13;count++) {
        int height=chrome_menu_height(kind,count);
        assert(chrome_menu_fits(kind,0,count-1,height));
        for (int limit=120;limit<=height;limit+=8) {
            int first=chrome_menu_last_first(kind,count,limit);
            assert(first>=0 && first<count && chrome_menu_fits(kind,first,count-1,limit));
            if (first) assert(!chrome_menu_fits(kind,first-1,count-1,limit));
        }
        for (int i=1;i<count;i++) assert(chrome_menu_offset(kind,i)-chrome_menu_offset(kind,i-1)==32+(chrome_menu_separator(kind,i) ? 8 : 0));
    }
    assert(chrome_menu_last_first(1,13,0)==12 && !chrome_menu_fits(1,12,12,0));
    assert(!chrome_menu_fits(1,0,-1,300));
    assert(chrome_menu_last_first(3,0,300)==0);
    return 0;
}
