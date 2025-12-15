// https://github.com/felix-u 2025-12-15
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(XML_H)
#define XML_H


typedef enum xml_Error {
    xml_Error_OK,
    xml_Error_SYNTAX,
    xml_Error_TOO_MANY_CLOSING_TAGS,
    xml_Error_UNCLOSED_TAGS,

    xml_Error_COUNT,
} xml_Error;

typedef struct xml_Reader {
    const char *data, *c, *end;
    int depth;
    xml_Error error;
} xml_Reader;

typedef enum xml_Type {
    xml_Type_TAG_CLOSE,
    xml_Type_TAG_OPEN,
    xml_Type_ATTRIBUTE,
    xml_Type_ERROR,
    xml_Type_END,

    xml_Type_COUNT,
} xml_Type;

typedef struct xml_Value {
    xml_Type type;
    const char *start, *end;
} xml_Value;

_Bool xml_read(xml_Reader *r, xml_Value *key, xml_Value *value);
xml_Reader xml_reader(const char *data, unsigned long long length);


#endif // XML_H


#if defined(XML_IMPLEMENTATION)


static _Bool xml__to_char(xml_Reader *r, char target) {
    for (; r->c < r->end; r->c += 1) {
        if (*r->c == '"' && target != '"') for (r->c += 1; r->c < r->end; r->c += 1) {
            if (*r->c == '"') {
                r->c += 1;
                break;
            }
        }
        if (*r->c == target) return 1;
    }
    return 0;
}

static _Bool xml__is_whitespace_char(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static _Bool xml__skip_whitespace(xml_Reader *r) {
    for (; r->c < r->end; r->c += 1) if (!xml__is_whitespace_char(*r->c)) return 1;
    return 0;
}

static _Bool xml__is_symbol_char(char c) {
    return !(c == '=' || xml__is_whitespace_char(c) || c == '<' || c == '>' || c == '"' || c == '/');
}

static _Bool xml__over_symbol(xml_Reader *r) {
    for (; r->c < r->end; r->c += 1) if (!xml__is_symbol_char(*r->c)) return 1;
    return 0;
}

_Bool xml_read(xml_Reader *r, xml_Value *key, xml_Value *value) {
    if (!xml__skip_whitespace(r)) return 0;

    static const char *key_start_backup_for_self_closing;
    static const char *key_end_backup_for_self_closing;

    switch (key->type) {
        case xml_Type_TAG_OPEN: case xml_Type_ATTRIBUTE: {
            _Bool is_attribute = xml__is_symbol_char(*r->c);
            if (is_attribute) {
                key->type = xml_Type_ATTRIBUTE;

                key->start = r->c;
                if (!xml__over_symbol(r)) break;
                key->end = r->c;

                if (!xml__skip_whitespace(r)) break;
                if (*r->c++ != '=') break;
                if (!xml__skip_whitespace(r)) break;
                if (*r->c++ != '"') break;

                value->type = xml_Type_ATTRIBUTE;
                value->start = r->c;
                if (!xml__to_char(r, '"')) break;
                value->end = r->c++;

                return 1;
            }

            _Bool self_closing = *r->c == '/';
            if (self_closing) {
                key->start = key_start_backup_for_self_closing;
                key->end = key_end_backup_for_self_closing;
                key->type = xml_Type_TAG_CLOSE;

                *value = (xml_Value){0};

                r->depth -= 1;
                r->c += 1;
                if (!xml__skip_whitespace(r)) break;
                if (*r->c != '>') break;
                r->c += 1;
                return 1;
            }
        } // fallthrough
        case xml_Type_TAG_CLOSE: {
            skip_special_tags: {
                if (!xml__skip_whitespace(r)) break;

                if (!xml__to_char(r, '<')) {
                    *key = *value = (xml_Value){0};
                    key->type = value->type = xml_Type_END;

                    if (r->depth < 0) {
                        r->error = xml_Error_TOO_MANY_CLOSING_TAGS;
                        break;
                    } else if (r->depth > 0) {
                        r->error = xml_Error_UNCLOSED_TAGS;
                        break;
                    }

                    return 0;
                }

                if (++r->c >= r->end) break;

                if (*r->c == '?' || *r->c == '!') {
                    if (!xml__to_char(r, '>')) break;
                    goto skip_special_tags;
                }
            }

            _Bool closing = r->c[0] == '/';
            if (closing && ++r->c >= r->end) break;

            key->start = r->c;
            if (!xml__over_symbol(r)) break;
            key->end = r->c;

            key_start_backup_for_self_closing = key->start;
            key_end_backup_for_self_closing = key->end;

            key->type = closing ? xml_Type_TAG_CLOSE : xml_Type_TAG_OPEN;
            r->depth += !closing - closing;
            *value = (xml_Value){0};
            return 1;
        } break;
        default: break;
    }

    *key = *value = (xml_Value){0};
    key->type = value->type = xml_Type_ERROR;
    if (r->error == xml_Error_OK) r->error = xml_Error_SYNTAX;
    return 0;
}

xml_Reader xml_reader(const char *data, unsigned long long length) {
    xml_Reader reader = {
        .data = data,
        .c = data,
        .end = data + length,
    };
    return reader;
}


#endif // XML_IMPLEMENTATION
