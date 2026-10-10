#ifndef ISSD_JSON_INTERNAL_H
#define ISSD_JSON_INTERNAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p;
    int line;
    const char *error;   /* first failure, or NULL */
    int error_line;
} JsonReader;

static inline void json_fail(JsonReader *r, const char *why) {
    if (!r->error) { r->error = why; r->error_line = r->line; }
}

static inline void json_skip_ws(JsonReader *r) {
    for (;;) {
        const char c = *r->p;
        if (c == '\n') { r->line++; r->p++; }
        else if (c == ' ' || c == '\t' || c == '\r') r->p++;
        /* Line and block comments are not JSON, but hand-written packs
         * have always had them and the old scanner skipped them. */
        else if (c == '/' && r->p[1] == '/') { while (*r->p && *r->p != '\n') r->p++; }
        else if (c == '/' && r->p[1] == '*') {
            r->p += 2;
            while (*r->p && !(*r->p == '*' && r->p[1] == '/')) {
                if (*r->p == '\n') r->line++;
                r->p++;
            }
            if (*r->p) r->p += 2;
        }
        else return;
    }
}

static inline bool json_eat(JsonReader *r, char c) {
    json_skip_ws(r);
    if (*r->p != c) return false;
    r->p++;
    return true;
}

/* Reads a string into `out` when given, or skips it. Escapes are resolved
 * for the handful JSON defines; \u is accepted and becomes '?', since the
 * cartridge has no characters outside A-Z anyway. */
static inline bool json_string(JsonReader *r, char *out, size_t cap) {
    if (!json_eat(r, '"')) { json_fail(r, "expected a string"); return false; }
    size_t n = 0;
    while (*r->p && *r->p != '"') {
        char c = *r->p++;
        if (c == '\n') r->line++;
        if (c == '\\' && *r->p) {
            const char esc = *r->p++;
            switch (esc) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'u':
                    for (int i = 0; i < 4 && *r->p; i++) r->p++;
                    c = '?';
                    break;
                default: c = esc; break;   /* \" \\ \/ and anything else */
            }
        }
        if (out && n + 1 < cap) out[n++] = c;
    }
    if (out && cap) out[n] = '\0';
    if (*r->p != '"') { json_fail(r, "unterminated string"); return false; }
    r->p++;
    return true;
}

static inline bool json_number(JsonReader *r, long *out) {
    json_skip_ws(r);
    char *end = NULL;
    const double v = strtod(r->p, &end);
    if (end == r->p) { json_fail(r, "expected a number"); return false; }
    r->p = end;
    if (out) *out = (long)v;
    return true;
}

static inline bool json_skip_value(JsonReader *r);

/* Runs `body` for each "key": value of an object. */
typedef bool (*JsonMember)(JsonReader *r, const char *key, void *ctx);

static inline bool json_object(JsonReader *r, JsonMember body, void *ctx) {
    if (!json_eat(r, '{')) { json_fail(r, "expected an object"); return false; }
    json_skip_ws(r);
    if (json_eat(r, '}')) return true;
    for (;;) {
        char key[64];
        if (!json_string(r, key, sizeof key)) return false;
        if (!json_eat(r, ':')) { json_fail(r, "expected ':'"); return false; }
        if (!body(r, key, ctx)) return false;
        json_skip_ws(r);
        if (json_eat(r, ',')) { json_skip_ws(r); continue; }
        if (json_eat(r, '}')) return true;
        json_fail(r, "expected ',' or '}'");
        return false;
    }
}

/* Runs `body` for each element of an array. */
typedef bool (*JsonElement)(JsonReader *r, void *ctx);

static inline bool json_array(JsonReader *r, JsonElement body, void *ctx) {
    if (!json_eat(r, '[')) { json_fail(r, "expected an array"); return false; }
    json_skip_ws(r);
    if (json_eat(r, ']')) return true;
    for (;;) {
        if (!body(r, ctx)) return false;
        json_skip_ws(r);
        if (json_eat(r, ',')) { json_skip_ws(r); continue; }
        if (json_eat(r, ']')) return true;
        json_fail(r, "expected ',' or ']'");
        return false;
    }
}

static inline bool json_skip_member(JsonReader *r, const char *key, void *ctx) {
    (void)key; (void)ctx;
    return json_skip_value(r);
}
static inline bool json_skip_element(JsonReader *r, void *ctx) {
    (void)ctx;
    return json_skip_value(r);
}

static inline bool json_skip_value(JsonReader *r) {
    json_skip_ws(r);
    switch (*r->p) {
        case '"': return json_string(r, NULL, 0);
        case '{': return json_object(r, json_skip_member, NULL);
        case '[': return json_array(r, json_skip_element, NULL);
        case 't': if (strncmp(r->p, "true", 4)) return false; r->p += 4; return true;
        case 'f': if (strncmp(r->p, "false", 5)) return false; r->p += 5; return true;
        case 'n': if (strncmp(r->p, "null", 4)) return false; r->p += 4; return true;
        default: return json_number(r, NULL);
    }
}

#endif
