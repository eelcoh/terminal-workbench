#include <assert.h>
#include "text_edit.h"
int main(void) {
    char value[64]="naïve λ project";TextEdit edit;
    text_edit_init(&edit,value,true);
    text_edit_move(&edit,value,-1,false,false);assert(edit.caret==0 && !text_edit_selected(edit));
    text_edit_move(&edit,value,1,false,false);text_edit_move(&edit,value,1,false,false);
    text_edit_move(&edit,value,1,false,true);assert(edit.caret==4 && edit.anchor==2);
    assert(text_edit_replace(&edit,value,sizeof(value),"é",2));assert(!strcmp(value,"naéve λ project"));
    assert(text_edit_delete(&edit,value,sizeof(value),true,false));assert(!strcmp(value,"nave λ project") && edit.caret==2);
    assert(text_edit_delete(&edit,value,sizeof(value),false,false));assert(!strcmp(value,"nae λ project"));
    text_edit_position(&edit,strlen(value),false);text_edit_move(&edit,value,-1,true,true);
    assert(!strcmp(value+text_edit_low(edit),"project"));
    char before[64];strcpy(before,value);TextEdit previous=edit;
    assert(!text_edit_replace(&edit,value,sizeof(value),"bad\nvalue",9));
    assert(!text_edit_replace(&edit,value,sizeof(value),"\xc0\x80",2));
    assert(!text_edit_replace(&edit,value,sizeof(value),"\xed\xa0\x80",3));
    assert(!text_edit_replace(&edit,value,sizeof(value),"\xf4\x90\x80\x80",4));
    char oversized[64];memset(oversized,'x',sizeof(oversized));assert(!text_edit_replace(&edit,value,sizeof(value),oversized,sizeof(oversized)));
    assert(!strcmp(value,before) && edit.caret==previous.caret && edit.anchor==previous.anchor);
    assert(text_edit_replace(&edit,value,sizeof(value),"work",4));assert(!strcmp(value,"nae λ work"));
    strcpy(value,"/projects/λ/topic");text_edit_init(&edit,value,false);
    text_edit_move(&edit,value,-1,true,false);assert(!strcmp(value+edit.caret,"topic"));
    text_edit_word_select(&edit,value,strlen("/projects/"));assert(edit.anchor==10 && edit.caret==12);
    assert(text_edit_replace(&edit,value,sizeof(value),"β",2));assert(!strcmp(value,"/projects/β/topic"));
    text_edit_position(&edit,0,false);assert(text_edit_delete(&edit,value,sizeof(value),true,false));assert(edit.caret==0);
    text_edit_init(&edit,value,false);assert(text_edit_delete(&edit,value,sizeof(value),false,false));
    char small[8]="abcde";text_edit_init(&edit,small,false);text_edit_position(&edit,2,false);text_edit_position(&edit,4,true);
    assert(text_edit_replace(&edit,small,sizeof(small),"XYZ",3) && !strcmp(small,"abXYZe"));
    text_edit_init(&edit,small,true);assert(text_edit_replace(&edit,small,sizeof(small),"λ🙂",6));
    text_edit_init(&edit,small,false);text_edit_move(&edit,small,-1,false,false);assert(edit.caret==2);
    text_edit_move(&edit,small,-1,false,true);assert(edit.caret==0 && edit.anchor==2);
    assert(text_edit_delete(&edit,small,sizeof(small),false,false) && !strcmp(small,"🙂"));
    return 0;
}
