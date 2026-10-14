/* Presentation-only menu spacing; separators never get action/focus indices. */
#ifndef WORKBENCH_CHROME_MENU_LAYOUT_H
#define WORKBENCH_CHROME_MENU_LAYOUT_H
#include <stdbool.h>
static inline bool chrome_menu_separator(int kind,int index)
{return kind==1 ? index==2 || index==5 || index==10 || index==12 || index==13 : kind==2 && (index==2 || index==6 || index==7);}
static inline int chrome_menu_offset(int kind,int index)
{
    int offset=index*32;
    for (int i=1;i<=index;i++) if (chrome_menu_separator(kind,i)) offset+=8;
    return offset;
}
static inline int chrome_menu_height(int kind,int count)
{return count ? chrome_menu_offset(kind,count-1)+44 : 12;}
static inline bool chrome_menu_fits(int kind,int first,int index,int height)
{return index>=first && chrome_menu_offset(kind,index)-chrome_menu_offset(kind,first)+30<=height-12;}
static inline int chrome_menu_last_first(int kind,int count,int height)
{
    int first=0;
    while (first+1<count && !chrome_menu_fits(kind,first,count-1,height)) first++;
    return first;
}
#endif
