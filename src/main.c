// useful links:
// https://ruffle.rs/demo/
// https://www.w3.org/TR/SVG2/

#define PLATFORM_NONE 1
#define BASE_IMPLEMENTATION
#include "base/base.h"

static void program(void) {
    Arena arena = arena_init(8 * 1024 * 1024);

    Slice_String args = os_get_arguments(&arena);
    if (args.count != 3) {
        log_error("need two (2) arguments\nusage: %S <svg_input> <swf_output>", args.data[0]);
        os_exit(1);
    }

    String svg_path = args.data[1];
    String swf_path = args.data[2];

    String svg = os_read_entire_file(&arena, cstring_from_string(&arena, svg_path), 0);
    if (svg.count == 0) {
        log_error("failure reading file '%S'", svg_path);
        os_exit(1);
    }

    String swf_header = string_print(&arena,
        "F" // uncompressed
        "WS"
        "\x13" // single-byte version (19)
        "\0\0\0\x10" // [u32] length of file in bytes, including this header
        "\0\0\0\0" // [RECT] frame size
        "\0\0" // [u16] framerate
        "\0\x01" // [u16] frame count
    );
    String swf = swf_header; // TODO(felix)
    
    // u8 *swf_length_in_header = &swf_header.data[4];
    // for (u64 i = 0; i < 4; i += 1)
    //     swf_length_in_header[i] = (u8)(swf.count >> (3 - i));

    bool ok = os_write_entire_file(cstring_from_string(&arena, swf_path), swf);
    if (!ok) os_exit(1);
}
