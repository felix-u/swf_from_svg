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

static i32 twips_from_pixels(f32 pixels) {
    i32 result = (i32)(pixels * 20.f + 0.5f);
    return result;
}

structdef(SWF_Rect) { u8 bytes[17]; };
static SWF_Rect swf_rect(i32 x_min, i32 x_max, i32 y_min, i32 y_max) {
    SWF_Rect rect = {0};
    u8 *r = rect.bytes;

    u8 bits_per_field = 32;
    r[0] |= bits_per_field << 3;

    i32 values[4] = { x_min, x_max, y_min, y_max };

    u64 r_bit = 5;
    for (u64 value_index = 0; value_index < 4; value_index += 1) {
        u32 unsigned_value = bit_cast(u32) values[value_index];
        for (i32 bit = 31; bit >= 0; bit -= 1, r_bit += 1) {
            u64 r_byte = r_bit >> 3;
            u64 r_bit_in_byte = 7 - (r_bit & 7);
            u8 b = (u8)((unsigned_value >> bit) & 1);
            r[r_byte] |= (u8)(b << r_bit_in_byte);
        }
    }

    return rect;
}

typedef enum SVG_Part_Kind {
    SVG_Part_Kind_PATH,
    SVG_Part_Kind_ELLIPSE,
    SVG_Part_Kind_RECT,

    SVG_Part_Kind_COUNT,
} SVG_Part_Kind;

structdef(SVG_Part) {
    SVG_Part_Kind kind;
    u32 fill_rgba;
    f32 stroke_width;
    union {
        struct {
            int TODO;
        } path;
        struct {
            V2 centre, radius;
        } ellipse;
        struct {
            V2 position, size;
        } rect;
    };
};

static bool xml_read_with_strings(xml_Reader *r, xml_Value *key, xml_Value *value, String *key_string, String *value_string) {
    bool result = xml_read(r, key, value);
    *key_string = string_from_xml(*key);
    *value_string = string_from_xml(*value);
    return result;
}

static void svg_part_parse_style(SVG_Part *part, String style) {
    {
        assert(string_starts_with(style, string("fill:")));

        u64 fill_start = string("fill:").count;

        u64 fill_end = fill_start;
        while (fill_end < style.count && style.data[fill_end] != ';') fill_end += 1;
        assert(fill_end < style.count);

        String fill = slice_range(style, fill_start, fill_end);

        if (fill.data[0] == '#') {
            String rgb_string = string_range(fill, 1, fill.count);
            u32 rgb = (u32)int_from_string_base(rgb_string, 16);
            part->fill_rgba = (rgb << 8) | 0xff;
        } else {
            assert(string_equals(fill, string("none")));
            part->fill_rgba = 0;
        }
    }
    {
        String needle = string("stroke-width:");
        u64 stroke_width_start = 0;
        for (u64 i = 0; i < style.count - needle.count; i += 1) {
            String shift = string_range(style, i, style.count);
            if (string_starts_with(shift, needle)) {
                stroke_width_start = i + needle.count;
                break;
            }
        }
        assert(stroke_width_start != 0);

        u64 stroke_width_end = stroke_width_start;
        while (stroke_width_end < style.count && style.data[stroke_width_end] != ';') stroke_width_end += 1;
        assert(stroke_width_end < style.count);

        String stroke_width_string = string_range(style, stroke_width_start, stroke_width_end);

        part->stroke_width = (f32)f64_from_string(stroke_width_string);
    }
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

    Array_SVG_Part svg_parts = { .arena = &arena };

    f32 svg_width = 0;
    f32 svg_height = 0;

    {
        xml_Reader r = xml_reader((const char *)svg.data, svg.count);
        xml_Value key = {0}, value = {0};
        String key_string = {0}, value_string = {0};
        while (xml_read_with_strings(&r, &key, &value, &key_string, &value_string)) {
            if (key.type == xml_Type_TAG_OPEN && string_equals(key_string, string("svg"))) {
                while (xml_read_with_strings(&r, &key, &value, &key_string, &value_string)) {
                    if (key.type != xml_Type_ATTRIBUTE) continue;
                    assert(value.type == xml_Type_ATTRIBUTE);

                    f32 *parse_f32 = 0;
                    if (string_equals(key_string, string("width"))) parse_f32 = &svg_width;
                    else if (string_equals(key_string, string("height"))) parse_f32 = &svg_height;
                    if (parse_f32 != 0) *parse_f32 = (f32)f64_from_string(value_string);

                    bool done_here = svg_width != 0 && svg_height != 0;
                    if (done_here) break;
                }
            }

            if (key.type != xml_Type_TAG_OPEN) continue;

            _Bool relevant = string_equals(key_string, string("path")) || string_equals(key_string, string("ellipse")) || string_equals(key_string, string("rect"));
            if (!relevant) continue;

            SVG_Part part = {0};
            u8 svg_kind = key_string.data[0];

            while (xml_read_with_strings(&r, &key, &value, &key_string, &value_string)) {
                if (key.type == xml_Type_ATTRIBUTE && string_equals(key_string, string("style"))) {
                    svg_part_parse_style(&part, value_string);
                    break;
                }
            }

            switch (svg_kind) {
                case 'p': {
                    part.kind = SVG_Part_Kind_PATH;
                    while (xml_read_with_strings(&r, &key, &value, &key_string, &value_string)) {
                        if (key.type == xml_Type_TAG_CLOSE) break;

                        if (string_equals(key_string, string("d"))) {
                            log_info("TODO(felix): PATH unimplemented");
                        }
                    }
                } break;
                case 'e': {
                    part.kind = SVG_Part_Kind_ELLIPSE;

                    String cx_string = {0};
                    String cy_string = {0};
                    String rx_string = {0};
                    String ry_string = {0};

                    while (xml_read_with_strings(&r, &key, &value, &key_string, &value_string)) {
                        if (key.type == xml_Type_TAG_CLOSE) break;

                        if (string_equals(key_string, string("cx"))) cx_string = value_string;
                        else if (string_equals(key_string, string("cy"))) cy_string = value_string;
                        else if (string_equals(key_string, string("rx"))) rx_string = value_string;
                        else if (string_equals(key_string, string("ry"))) ry_string = value_string;
                    }

                    part.ellipse.centre.x = (f32)f64_from_string(rx_string);
                    part.ellipse.centre.y = (f32)f64_from_string(ry_string);
                    part.ellipse.radius.x = (f32)f64_from_string(cx_string);
                    part.ellipse.radius.y = (f32)f64_from_string(cy_string);
                } break;
                case 'r': {
                    part.kind = SVG_Part_Kind_RECT;

                    String width_string = {0};
                    String height_string = {0};
                    String x_string = {0};
                    String y_string = {0};

                    while (xml_read_with_strings(&r, &key, &value, &key_string, &value_string)) {
                        if (key.type == xml_Type_TAG_CLOSE) break;

                        if (string_equals(key_string, string("width"))) width_string = value_string;
                        else if (string_equals(key_string, string("height"))) height_string = value_string;
                        else if (string_equals(key_string, string("x"))) x_string = value_string;
                        else if (string_equals(key_string, string("y"))) y_string = value_string;
                    }

                    part.rect.position.x = (f32)f64_from_string(x_string);
                    part.rect.position.y = (f32)f64_from_string(y_string);
                    part.rect.size.x = (f32)f64_from_string(width_string);
                    part.rect.size.y = (f32)f64_from_string(height_string);
                } break;
                default: unreachable;
            }

            push(&svg_parts, part);
        }
        assert(r.error == xml_Error_OK);
    }

    String_Builder swf = { .arena = &arena };
    string_builder_print(&swf, "%s",
        "F" // uncompressed
        "WS" // signature bytes
        "\x06" // single-byte version (6)
        "0000" // [u32] length of file in bytes, including this header (filled later)
        "00001000200030004" // [RECT] frame size, 17 bytes = 5 bits + 32 bits x 4 values, padded to the next full byte (filled later)
        "\x01\0" // [u16] framerate (1)
        "\x01\0" // [u16] frame count (1)
    );
    u8 *swf_length_in_header = &swf.data[4];
    u8 *frame_size_rect_in_header = &swf.data[8];

    for (u64 i = 0; i < 4; i += 1) swf_length_in_header[i] = (u8)(swf.count >> (8 * i));

    SWF_Rect frame_size = swf_rect(0, twips_from_pixels(svg_width), 0, twips_from_pixels(svg_height));
    memcpy(frame_size_rect_in_header, frame_size.bytes, sizeof frame_size.bytes);

    // // NOTE(felix): this is a known valid SWF file given in the spec Appendix A
    // swf = string(
    //     "\x46\x57\x53\x03\x4F\x00\x00\x00"
    //     "\x78\x00\x05\x5F\x00\x00\x0F\xA0"
    //     "\x00\x00\x0C\x01\x00\x43\x02\xFF"
    //     "\xFF\xFF\xBF\x00\x23\x00\x00\x00"
    //     "\x01\x00\x70\xFB\x49\x97\x0D\x0C"
    //     "\x7D\x50\x00\x01\x14\x00\x00\x00"
    //     "\x00\x01\x25\xC9\x92\x0D\x21\xED"
    //     "\x48\x87\x65\x30\x3B\x6D\xE1\xD8"
    //     "\xB4\x00\x00\x86\x06\x06\x01\x00"
    //     "\x01\x00\x00\x40\x00\x00\x00"
    // );

    bool ok = os_write_entire_file(cstring_from_string(&arena, swf_path), swf.string);
    if (!ok) os_exit(1);
}
