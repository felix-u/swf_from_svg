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

static bool xml_read_with_strings(xml_Reader *r, xml_Value *key, xml_Value *value, String *key_string, String *value_string) {
    bool result = xml_read(r, key, value);
    *key_string = string_from_xml(*key);
    *value_string = string_from_xml(*value);
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

    u8 bits_per_field = 31;
    assert(bits_per_field <= 31);
    r[0] |= bits_per_field << 3;

    i32 values[4] = { x_min, x_max, y_min, y_max };

    u64 r_bit = 5;
    for (u64 value_index = 0; value_index < 4; value_index += 1) {
        u32 unsigned_value = bit_cast(u32) values[value_index];
        for (i32 bit = bits_per_field - 1; bit >= 0; bit -= 1, r_bit += 1) {
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
            using(V2, position);
            V2 size;
        } rect;
    };
};

typedef enum SWF_Tag_Type {
    SWF_Tag_Type_END          =  0,
    SWF_Tag_Type_SHOWFRAME    =  1,
    SWF_Tag_Type_PLACEOBJECT2 = 26,
    SWF_Tag_Type_DEFINESHAPE3 = 32,
} SWF_Tag_Type;

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

typedef u16 SWF_Record_Header;

static void swf_write_u16(String_Builder *swf, u16 value) {
    push(swf, (u8)value);
    push(swf, (u8)(value >> 8));
}

static void swf_write_u32(String_Builder *swf, u32 value) {
    push(swf, (u8)value);
    push(swf, (u8)(value >> 8));
    push(swf, (u8)(value >> 16));
    push(swf, (u8)(value >> 24));
}

structdef(SWF_Fill_Style) {
    u8 type;
    u32 color;
};

structdef(SWF_Line_Style) {
    u16 width_twips;
    u32 color;
};

structdef(SWF_Shape_With_Style) {
    SWF_Fill_Style fill_style;
    SWF_Line_Style line_style;
};

static u32 swf_sbits_width(i32 v) {
    for (u32 n = 1; n <= 32; n += 1) {
        i64 minv = -((i64)1 << (n - 1));
        i64 maxv =  ((i64)1 << (n - 1)) - 1;
        if ((i64)v >= minv && (i64)v <= maxv) return n;
    }
    return 32;
}

typedef struct SWF_Bit_Writer {
    String_Builder *swf;
    u8 byte;
    u32 bit_count; /* 0..7 bits currently in 'byte' (from MSB downward) */
} SWF_Bit_Writer;

static void swf_bw_push_bit(SWF_Bit_Writer *w, u32 bit) {
    u32 shift = 7 - (w->bit_count & 7);
    w->byte |= (u8)((bit & 1) << shift);
    w->bit_count += 1;
    if ((w->bit_count & 7) == 0) {
        push(w->swf, w->byte);
        w->byte = 0;
    }
}

static void swf_bw_push_ubits(SWF_Bit_Writer *w, u32 v, u32 nbits) {
    for (i32 bit = (i32)nbits - 1; bit >= 0; bit -= 1) {
        swf_bw_push_bit(w, (v >> bit) & 1u);
    }
}

static void swf_bw_push_sbits(SWF_Bit_Writer *w, i32 v, u32 nbits) {
    u32 uv = (u32)v;
    for (i32 bit = (i32)nbits - 1; bit >= 0; bit -= 1) {
        swf_bw_push_bit(w, (uv >> bit) & 1u);
    }
}

static void swf_bw_byte_align(SWF_Bit_Writer *w) {
    if ((w->bit_count & 7) != 0) {
        push(w->swf, w->byte);
        w->byte = 0;
        w->bit_count = (w->bit_count + 7) & ~7u;
    }
}

static void swf_push_shapewithstyle(String_Builder *swf, SWF_Shape_With_Style shapes, SVG_Part part) {
    // FILLSTYLEARRAY
    push(swf, 1); // count
    { // FILLSTYLE
        assert(shapes.fill_style.type == 0); // solid
        push(swf, shapes.fill_style.type);

        u32 rgba = shapes.fill_style.color;
        push(swf, (u8)(rgba >> 24));
        push(swf, (u8)(rgba >> 16));
        push(swf, (u8)(rgba >> 8));
        push(swf, (u8)(rgba >> 0));
    }

    // LINESTYLEARRAY
    push(swf, 1); // count
    {
        swf_write_u16(swf, shapes.line_style.width_twips);

        u32 rgba = shapes.line_style.color;
        push(swf, (u8)(rgba >> 24));
        push(swf, (u8)(rgba >> 16));
        push(swf, (u8)(rgba >> 8));
        push(swf, (u8)(rgba >> 0));
    }

    push(swf, 0x11); // NumFillBits=1 (high nibble), NumLineBits=1 (low nibble)

    assert(part.kind == SVG_Part_Kind_RECT);

    {
        i32 x0 = twips_from_pixels(part.rect.position.x);
        i32 y0 = twips_from_pixels(part.rect.position.y);
        i32 w  = twips_from_pixels(part.rect.size.x);
        i32 h  = twips_from_pixels(part.rect.size.y);

        SWF_Bit_Writer bw = { .swf = swf, .byte = 0, .bit_count = 0 };

        /* StyleChangeRecord: MoveTo + FillStyle0=1 + LineStyle=1 */
        swf_bw_push_bit(&bw, 0); /* TypeFlag: non-edge */

        swf_bw_push_bit(&bw, 0); /* StateNewStyles */
        swf_bw_push_bit(&bw, 1); /* StateLineStyle */
        swf_bw_push_bit(&bw, 0); /* StateFillStyle1 */
        swf_bw_push_bit(&bw, 1); /* StateFillStyle0 */
        swf_bw_push_bit(&bw, 1); /* StateMoveTo */

        {
            u32 mx = swf_sbits_width(x0);
            u32 my = swf_sbits_width(y0);
            u32 move_bits = (mx > my) ? mx : my;
            if (move_bits < 1) move_bits = 1;
            if (move_bits > 31) move_bits = 31;

            swf_bw_push_ubits(&bw, move_bits, 5);
            swf_bw_push_sbits(&bw, x0, move_bits);
            swf_bw_push_sbits(&bw, y0, move_bits);
        }

        swf_bw_push_ubits(&bw, 1, 1); /* FillStyle0 index (NumFillBits=1) */
        swf_bw_push_ubits(&bw, 1, 1); /* LineStyle index (NumLineBits=1) */

        /* 4 StraightEdgeRecords (axis-aligned) */
        {
            i32 dx[4] = { +w,  0, -w,  0 };
            i32 dy[4] = {  0, +h,  0, -h };

            for (u32 e = 0; e < 4; e += 1) {
                i32 edx = dx[e];
                i32 edy = dy[e];

                swf_bw_push_bit(&bw, 1); /* TypeFlag: edge */
                swf_bw_push_bit(&bw, 1); /* StraightFlag: straight */

                if (edy == 0) {
                    u32 n = swf_sbits_width(edx);
                    if (n < 2) n = 2;
                    swf_bw_push_ubits(&bw, n - 2, 4); /* NumBits */
                    swf_bw_push_bit(&bw, 0);          /* GeneralLineFlag */
                    swf_bw_push_bit(&bw, 0);          /* VertLineFlag=0 => horizontal */
                    swf_bw_push_sbits(&bw, edx, n);   /* DeltaX */
                } else {
                    u32 n = swf_sbits_width(edy);
                    if (n < 2) n = 2;
                    swf_bw_push_ubits(&bw, n - 2, 4); /* NumBits */
                    swf_bw_push_bit(&bw, 0);          /* GeneralLineFlag */
                    swf_bw_push_bit(&bw, 1);          /* VertLineFlag=1 => vertical */
                    swf_bw_push_sbits(&bw, edy, n);   /* DeltaY */
                }
            }
        }

        /* EndShapeRecord: 0 + five 0 flags */
        swf_bw_push_bit(&bw, 0);
        swf_bw_push_ubits(&bw, 0, 5);

        swf_bw_byte_align(&bw);
    }

    // TODO(felix): StyleChangeRecord: select fill & line styles, move to x0,y0

    // TODO(felix): 4 StraightEdgeRecord s: to x1,y0, then x1,y1, then x0,y1, then back to x0,y0

    // TODO(felix): EndShapeRecord
}

static void swf_push_defineshape3(String_Builder *swf, u16 shape_id, SWF_Rect shape_bounds, SWF_Shape_With_Style shapes, SVG_Part part) {
    assert(shape_id != 0);

    u64 tag_start = swf->count;
    swf_write_u16(swf, (u16)((SWF_Tag_Type_DEFINESHAPE3 << 6) | 0x3f));
    u64 length_patch_at = swf->count;
    swf_write_u32(swf, 0);

    swf_write_u16(swf, shape_id);
    for (u64 i = 0; i < sizeof shape_bounds.bytes; i += 1) push(swf, shape_bounds.bytes[i]);
    swf_push_shapewithstyle(swf, shapes, part);

    u64 body_length = swf->count - (length_patch_at + 4);
    assert(body_length <= 0xffffffffu);
    u32 body_length_u32 = (u32)body_length;
    swf->data[length_patch_at + 0] = (u8)(body_length_u32 >> 0);
    swf->data[length_patch_at + 1] = (u8)(body_length_u32 >> 8);
    swf->data[length_patch_at + 2] = (u8)(body_length_u32 >> 16);
    swf->data[length_patch_at + 3] = (u8)(body_length_u32 >> 24);

    assert((swf->count - tag_start) == (2 + 4 + body_length));
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
                    String cx_string = {0}, cy_string = {0}, rx_string = {0}, ry_string = {0};

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
                    String width_string = {0}, height_string = {0}, x_string = {0}, y_string = {0};

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

    u8 *frame_size_rect_in_header = &swf.data[8];
    SWF_Rect frame_size = swf_rect(0, twips_from_pixels(svg_width), 0, twips_from_pixels(svg_height));
    assert((frame_size.bytes[0] >> 3) != 0);
    memcpy(frame_size_rect_in_header, frame_size.bytes, sizeof frame_size.bytes);

    u16 next_shape_id = 1;
    u16 next_depth = 1;
    for (u64 i = 0; i < svg_parts.count; i += 1) {
        SVG_Part *part = &svg_parts.data[i];

        switch (part->kind) {
            case SVG_Part_Kind_PATH: {
                log_info("TODO(felix): PATH unimplemented");
            } break;
            case SVG_Part_Kind_ELLIPSE: {
                log_info("TODO(felix): ELLIPSE unimplemented");
            } break;
            case SVG_Part_Kind_RECT: {
                assert(next_shape_id != 0);
                assert(next_depth != 0);

                u16 shape_id = next_shape_id++;
                u16 depth = next_depth++;

                i32 x0 = twips_from_pixels(part->rect.position.x);
                i32 y0 = twips_from_pixels(part->rect.position.y);
                i32 x1 = x0 + twips_from_pixels(part->rect.size.x);
                i32 y1 = y0 + twips_from_pixels(part->rect.size.y);

                SWF_Rect shape_bounds = swf_rect(x0, x1, y0, y1);

                SWF_Shape_With_Style shapes = {0};
                shapes.fill_style.type = 0;
                shapes.fill_style.color = part->fill_rgba;

                {
                    i32 stroke_twips = twips_from_pixels(part->stroke_width);
                    if (stroke_twips < 0) stroke_twips = 0;
                    assert(stroke_twips <= 0xffff);
                    shapes.line_style.width_twips = (u16)stroke_twips;
                }
                shapes.line_style.color = (part->fill_rgba != 0) ? part->fill_rgba : 0x000000ff;

                swf_push_defineshape3(&swf, shape_id, shape_bounds, shapes, *part);

                {
                    u16 body_length = 1 + 2 + 2 + 1;
                    u16 tag_code_and_length = (u16)((SWF_Tag_Type_PLACEOBJECT2 << 6) | body_length);
                    swf_write_u16(&swf, tag_code_and_length);

                    u8 flags = 0;
                    flags |= (1u << 2); /* HasMatrix */
                    flags |= (1u << 1); /* HasCharacter */
                    push(&swf, flags);

                    swf_write_u16(&swf, depth);
                    swf_write_u16(&swf, shape_id);
                    push(&swf, 0); /* empty MATRIX */
                }
            } break;
            default: unreachable;
        }
    }

    swf_write_u16(&swf, (u16)((SWF_Tag_Type_SHOWFRAME << 6) | 0));
    swf_write_u16(&swf, (u16)((SWF_Tag_Type_END << 6) | 0));

    if (BUILD_DEBUG) {
        // NOTE(felix): this is a known valid SWF file given in the spec Appendix A
        const char *path = "official_example.swf";
        if (!os_file_info(path).exists) {
            String official_example = string(
                "\x46\x57\x53\x03\x4F\x00\x00\x00"
                "\x78\x00\x05\x5F\x00\x00\x0F\xA0"
                "\x00\x00\x0C\x01\x00\x43\x02\xFF"
                "\xFF\xFF\xBF\x00\x23\x00\x00\x00"
                "\x01\x00\x70\xFB\x49\x97\x0D\x0C"
                "\x7D\x50\x00\x01\x14\x00\x00\x00"
                "\x00\x01\x25\xC9\x92\x0D\x21\xED"
                "\x48\x87\x65\x30\x3B\x6D\xE1\xD8"
                "\xB4\x00\x00\x86\x06\x06\x01\x00"
                "\x01\x00\x00\x40\x00\x00\x00"
            );
            os_write_entire_file(path, official_example);
        }
    }

    u8 *swf_length_in_header = &swf.data[4];
    for (u64 i = 0; i < 4; i += 1) swf_length_in_header[i] = (u8)(swf.count >> (8 * i));

    bool ok = os_write_entire_file(cstring_from_string(&arena, swf_path), swf.string);
    if (!ok) os_exit(1);
}
