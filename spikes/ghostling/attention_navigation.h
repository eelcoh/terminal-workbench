/* Bounded presentation order; no runtime/PTY access or automatic focus changes. */
#ifndef WORKBENCH_ATTENTION_NAVIGATION_H
#define WORKBENCH_ATTENTION_NAVIGATION_H
#include <stdbool.h>
#include <stddef.h>
#define ATTENTION_LIMIT 64
static inline int attention_next(const bool *waiting,size_t count,int current,int direction)
{
    if(!waiting || !count || count>ATTENTION_LIMIT) return -1;
    int step=direction<0 ? -1 : 1,size=(int)count;
    int origin=current>=0 && current<size ? current : step>0 ? size-1 : 0;
    for(int distance=1;distance<=size;distance++) {
        int index=(origin+step*distance+size*2)%size;
        if(waiting[index]) return index;
    }
    return -1;
}
#endif
