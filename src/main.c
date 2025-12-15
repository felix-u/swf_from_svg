// useful links:
// https://ruffle.rs/demo/
// https://www.w3.org/TR/SVG2/

#define XML_IMPLEMENTATION
#include "xml.h"

#define PLATFORM_NONE 1
#define BASE_IMPLEMENTATION
#include "base/base.h"

static String string_from_xml(xml_Value value) {
    String result = {
        .data = (u8 *)value.start,
        .count = (u64)(value.end - value.start),
    };
    return result;
}

static u64 twips_from_pixels(f32 pixels) {
    u64 result = (u64)(pixels * 20.f + 0.5f);
    return result;
}

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

    f32 svg_width = 0;
    f32 svg_height = 0;

    xml_Reader r = xml_reader((const char *)svg.data, svg.count);
    xml_Value key = {0}, value = {0};
    while (xml_read(&r, &key, &value)) {
        String key_string = string_from_xml(key);
        String value_string = string_from_xml(value);

        if (key.type == xml_Type_TAG_OPEN && string_equals(key_string, string("svg"))) while (xml_read(&r, &key, &value)) {
            if (key.type != xml_Type_ATTRIBUTE) continue;
            assert(value.type == xml_Type_ATTRIBUTE);

            key_string = string_from_xml(key);
            value_string = string_from_xml(value);

            f32 *parse_f32 = 0;
            if (string_equals(key_string, string("width"))) parse_f32 = &svg_width;
            else if (string_equals(key_string, string("height"))) parse_f32 = &svg_height;
            if (parse_f32 != 0) *parse_f32 = (f32)f64_from_string(value_string);

            bool done_here = svg_width != 0 && svg_height != 0;
            if (done_here) break;
        }

        if (key.type != xml_Type_TAG_OPEN) continue;

        // TODO(felix): was figuring out how to output valid SWF

        if (string_equals(key_string, string("path"))) {
            // TODO(felix)
        }

        if (string_equals(key_string, string("ellipse"))) {
            // TODO(felix)
        }

        if (string_equals(key_string, string("rect"))) {
            // TODO(felix)
        }
    }
    assert(r.error == xml_Error_OK);

    discard swf_path;

    // String swf_header = string_print(&arena,
    //     "F" // uncompressed
    //     "WS"
    //     "\x13" // single-byte version (19)
    //     "\0\0\0\x10" // [u32] length of file in bytes, including this header
    //     "\0\0\0\0" // [RECT] frame size
    //     "\0\0" // [u16] framerate
    //     "\0\x01" // [u16] frame count
    // );
    // String swf = swf_header; // TODO(felix)

    // u8 *swf_length_in_header = &swf_header.data[4];
    // for (u64 i = 0; i < 4; i += 1)
    //     swf_length_in_header[i] = (u8)(swf.count >> (3 - i));

    // bool ok = os_write_entire_file(cstring_from_string(&arena, swf_path), swf);
    // if (!ok) os_exit(1);
}
