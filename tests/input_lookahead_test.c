#include <stdbool.h>
#include <assert.h>
static int keys[512], chars[512], key_read, char_read, key_total, char_total;
static int GetKeyPressed(void) { return key_read < key_total ? keys[key_read++] : 0; }
static int GetCharPressed(void) { return char_read < char_total ? chars[char_read++] : 0; }
#include "input_lookahead.h"
int main(void)
{
    keys[0] = 80; keys[1] = 257; key_total = 2;
    chars[0] = 'p'; chars[1] = 0x1f980; char_total = 2;
    workbench_input_peek();
    workbench_input_peek();
    assert(workbench_input_has_key(80));
    assert(!workbench_input_has_key(81));
    assert(workbench_input_has_text());
    assert(GetKeyPressed() == 80);
    assert(GetKeyPressed() == 257);
    assert(GetKeyPressed() == 0);
    assert(GetCharPressed() == 'p');
    assert(GetCharPressed() == 0x1f980);
    assert(GetCharPressed() == 0);
    assert(!workbench_input_has_text());
    // Bounded lookahead leaves later events in the original queue, in order.
    key_read = char_read = 0; key_total = char_total = 300;
    for (int i = 0; i < 300; i++) { keys[i] = i + 1; chars[i] = i + 1; }
    workbench_input_peek();
    assert(key_read == WORKBENCH_INPUT_LOOKAHEAD);
    assert(char_read == WORKBENCH_INPUT_LOOKAHEAD);
    for (int i = 0; i < 300; i++) {
        assert(GetKeyPressed() == i + 1);
        assert(GetCharPressed() == i + 1);
    }
    assert(GetKeyPressed() == 0 && GetCharPressed() == 0);
    // Empty lookahead does not prevent ordinary input bypassing the buffers.
    keys[key_total++] = 90; chars[char_total++] = 'z';
    assert(GetKeyPressed() == 90 && GetCharPressed() == 'z');
    return 0;
}
