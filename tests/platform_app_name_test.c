#include <assert.h>
#include <string.h>
#define __APPLE__ 1
static const char *name;
void setprogname(const char *value) {name=value;}
#include "platform_app_name.h"
int main(void) {platform_app_name();assert(name && !strcmp(name,"Terminal Workbench"));return 0;}
