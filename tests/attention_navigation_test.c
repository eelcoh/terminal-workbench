#include <assert.h>
#include "attention_navigation.h"
int main(void)
{
    bool waiting[ATTENTION_LIMIT]={false};
    assert(attention_next(NULL,0,0,1)==-1 && attention_next(waiting,0,0,-1)==-1);
    assert(attention_next(waiting,ATTENTION_LIMIT,-1,1)==-1);
    waiting[1]=waiting[5]=waiting[14]=true;
    assert(attention_next(waiting,15,0,1)==1 && attention_next(waiting,15,2,1)==5);
    assert(attention_next(waiting,15,1,-1)==14 && attention_next(waiting,15,14,1)==1);
    assert(attention_next(waiting,15,14,-1)==5 && attention_next(waiting,15,5,1)==14);
    assert(attention_next(waiting,15,-1,1)==1 && attention_next(waiting,15,-1,-1)==14);
    waiting[1]=waiting[5]=false;
    assert(attention_next(waiting,15,14,1)==14 && attention_next(waiting,15,14,-1)==14);
    waiting[14]=false;waiting[ATTENTION_LIMIT-1]=true;
    assert(attention_next(waiting,ATTENTION_LIMIT,0,1)==ATTENTION_LIMIT-1 && attention_next(waiting,ATTENTION_LIMIT,0,-1)==ATTENTION_LIMIT-1);
    assert(attention_next(waiting,ATTENTION_LIMIT+1,0,1)==-1);
    return 0;
}
