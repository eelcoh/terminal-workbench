/* Test-only physical keyboard for a private virtual-keyboard Wayland seat. */
#define _GNU_SOURCE
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "virtual-keyboard.h"
static struct zwp_virtual_keyboard_manager_v1 *manager;
static struct wl_seat *seat;
static void global(void *data,struct wl_registry *registry,uint32_t name,const char *interface,uint32_t version) {
    (void)data;(void)version;
    if(!strcmp(interface,"zwp_virtual_keyboard_manager_v1"))manager=wl_registry_bind(registry,name,&zwp_virtual_keyboard_manager_v1_interface,1);
    if(!strcmp(interface,"wl_seat"))seat=wl_registry_bind(registry,name,&wl_seat_interface,1);
}
static void removed(void *data,struct wl_registry *registry,uint32_t name) {(void)data;(void)registry;(void)name;}
int main(int argc,char **argv) {
    assert(argc>=3);
    struct wl_display *display=wl_display_connect(NULL);assert(display);
    struct wl_registry *registry=wl_display_get_registry(display);
    const struct wl_registry_listener listener={global,removed};
    wl_registry_add_listener(registry,&listener,NULL);assert(wl_display_roundtrip(display)>=0);assert(manager&&seat);
    struct zwp_virtual_keyboard_v1 *keyboard=zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(manager,seat);
    struct xkb_context *context=xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names names={.variant=getenv("WORKBENCH_TEST_KEYBOARD_VARIANT"),.layout=getenv("WORKBENCH_TEST_KEYBOARD_LAYOUT") ? getenv("WORKBENCH_TEST_KEYBOARD_LAYOUT") : "us"};
    struct xkb_keymap *map=xkb_keymap_new_from_names(context,&names,XKB_KEYMAP_COMPILE_NO_FLAGS);assert(map);
    char *serialized=xkb_keymap_get_as_string(map,XKB_KEYMAP_FORMAT_TEXT_V1);assert(serialized);
    int fd=memfd_create("wb-test-keymap",MFD_CLOEXEC);assert(fd>=0);
    size_t length=strlen(serialized)+1;assert(write(fd,serialized,length)==(ssize_t)length);
    zwp_virtual_keyboard_v1_keymap(keyboard,WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,fd,length);close(fd);free(serialized);
    assert(wl_display_roundtrip(display)>=0);
    uint32_t held=0;
    for(int i=3;i<argc;i++) {
        xkb_mod_index_t mod=xkb_keymap_mod_get_index(map,argv[i]);assert(mod!=XKB_MOD_INVALID);held|=1u<<mod;
    }
    for(int i=3;i<argc;i++) {
        uint32_t code=!strcmp(argv[i],"Control")?29:!strcmp(argv[i],"Shift")?42:!strcmp(argv[i],"Mod1")?56:125;
        zwp_virtual_keyboard_v1_key(keyboard,0,code,WL_KEYBOARD_KEY_STATE_PRESSED);
    }
    zwp_virtual_keyboard_v1_modifiers(keyboard,held,0,0,0);
    assert(wl_display_roundtrip(display)>=0);usleep(60000);
    int text=!strcmp(argv[1],"text"),sequence=!strcmp(argv[1],"sequence");
    assert(text||sequence||!strcmp(argv[1],"key"));
    size_t count=text||sequence?strlen(argv[2]):1;
    char *next=NULL,*token=sequence?strtok_r(argv[2]," ",&next):NULL;
    for(size_t i=0;i<count;i++) {
        if(sequence && !token)break;
        xkb_keysym_t wanted=text?(unsigned char)argv[2][i]:xkb_keysym_from_name(sequence?token:argv[2],XKB_KEYSYM_NO_FLAGS);
        xkb_keycode_t found=0;uint32_t modifiers=held;
        for(xkb_keycode_t code=xkb_keymap_min_keycode(map);code<=xkb_keymap_max_keycode(map)&&!found;code++)
            for(int level=0;level<2;level++) {
                const xkb_keysym_t *syms=NULL;
                if(xkb_keymap_key_get_syms_by_level(map,code,0,level,&syms)==1&&syms[0]==wanted) {
                    found=code;
                    if(level)modifiers|=1u<<xkb_keymap_mod_get_index(map,XKB_MOD_NAME_SHIFT);
                    break;
                }
            }
        assert(found>=8);
        zwp_virtual_keyboard_v1_modifiers(keyboard,modifiers,0,0,0);
        zwp_virtual_keyboard_v1_key(keyboard,0,found-8,WL_KEYBOARD_KEY_STATE_PRESSED);
        if(!text) { assert(wl_display_roundtrip(display)>=0);usleep(80000); }
        zwp_virtual_keyboard_v1_key(keyboard,0,found-8,WL_KEYBOARD_KEY_STATE_RELEASED);
        zwp_virtual_keyboard_v1_modifiers(keyboard,0,0,0,0);
        assert(wl_display_roundtrip(display)>=0);usleep(1000);
        if(sequence)token=strtok_r(NULL," ",&next);
    }
    assert(wl_display_roundtrip(display)>=0);usleep(60000);
    for(int i=argc-1;i>=3;i--) {
        uint32_t code=!strcmp(argv[i],"Control")?29:!strcmp(argv[i],"Shift")?42:!strcmp(argv[i],"Mod1")?56:125;
        zwp_virtual_keyboard_v1_key(keyboard,0,code,WL_KEYBOARD_KEY_STATE_RELEASED);
    }
    zwp_virtual_keyboard_v1_modifiers(keyboard,0,0,0,0);
    zwp_virtual_keyboard_v1_destroy(keyboard);assert(wl_display_roundtrip(display)>=0);
    xkb_keymap_unref(map);xkb_context_unref(context);wl_display_disconnect(display);return 0;
}
