#define PLATFORM_NONE 1
#define BASE_IMPLEMENTATION
#include "src/base/base.h"

#define APP_NAME "sfs"

static void program(void) {
    Arena arena = arena_init(64 * 1024 * 1024);
    u32 exit_code = build_default_everything(arena, string(APP_NAME), BASE_OS);
    os_exit(exit_code);
}
