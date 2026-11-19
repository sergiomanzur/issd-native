/* Cartridge packs replace the whole image or patch the user's own retail copy.
 * The rules being checked come from
 * docs/superpowers/specs/2026-11-19-rom-hack-packs-design.md: a 2,097,664-byte
 * buffer is treated as carrying a 512-byte copier header, an IPS patch only
 * ever applies to an image that hashes to the retail SHA-256, and a patch that
 * would expand or truncate the 2 MB image is refused with a reason.
 *
 * Everything except the retail cartridge itself is synthetic. The real file at
 * the repository root is read for its hash and header bytes only; no ROM
 * content is copied into a fixture. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "issd_cartridge.h"
#include "issd_mod.h"

#define RETAIL_PATH "International Superstar Soccer Deluxe (USA).sfc"
#define COPIER_HEADER 512u

/* issd_mod.c reaches for SNES work RAM even when a test only parses packs. */
uint8_t g_ram[0x20000];

/* Reads the user's cartridge into a fresh buffer. The tests that need the real
 * bytes are the ones that prove the retail hash guard is live. */
static uint8_t *load_retail(size_t *size) {
    FILE *f = fopen(RETAIL_PATH, "rb");
    if (!f) return NULL;
    uint8_t *buf = malloc(ISSD_RETAIL_SIZE);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, ISSD_RETAIL_SIZE, f);
    fclose(f);
    if (got != ISSD_RETAIL_SIZE) {
        free(buf);
        return NULL;
    }
    *size = got;
    return buf;
}

/* A small IPS writer. Records are big-endian as the format says; the buffer is
 * sized for the handful of records these tests build, not for a real patch. */
typedef struct {
    uint8_t bytes[512];
    size_t len;
} Patch;

static void put_bytes(Patch *p, const void *src, size_t n) {
    assert(p->len + n <= sizeof p->bytes);
    memcpy(p->bytes + p->len, src, n);
    p->len += n;
}

static void put_be16(Patch *p, unsigned v) {
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    put_bytes(p, b, sizeof b);
}

static void put_be24(Patch *p, unsigned v) {
    uint8_t b[3] = { (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    put_bytes(p, b, sizeof b);
}

static void ips_begin(Patch *p) {
    p->len = 0;
    put_bytes(p, "PATCH", 5);
}

static void ips_record(Patch *p, unsigned offset, const void *data,
                       unsigned len) {
    put_be24(p, offset);
    put_be16(p, len);
    put_bytes(p, data, len);
}

static void ips_rle(Patch *p, unsigned offset, unsigned run, uint8_t fill) {
    put_be24(p, offset);
    put_be16(p, 0);
    put_be16(p, run);
    put_bytes(p, &fill, 1);
}

static void ips_eof(Patch *p) {
    put_bytes(p, "EOF", 3);
}

/* --- retail stash: shape normalization ---------------------------------- */

static void test_retail_stash(void) {
    static uint8_t headered[ISSD_RETAIL_SIZE + COPIER_HEADER];
    memset(headered, 0xAA, COPIER_HEADER);
    memset(headered + COPIER_HEADER, 0x11, ISSD_RETAIL_SIZE);

    char why[160] = "";
    assert(issd_cartridge_stash_retail(headered, sizeof headered, why, sizeof why));

    size_t size = 0;
    const uint8_t *base = issd_cartridge_retail(&size);
    assert(base == headered + COPIER_HEADER &&
           "a copier header is dropped, not treated as cartridge data");
    assert(size == ISSD_RETAIL_SIZE);
    assert(base[0] == 0x11);

    static uint8_t plain[ISSD_RETAIL_SIZE];
    memset(plain, 0x22, sizeof plain);
    assert(issd_cartridge_stash_retail(plain, sizeof plain, why, sizeof why));
    base = issd_cartridge_retail(&size);
    assert(base == plain && size == ISSD_RETAIL_SIZE);

    /* One byte more than the headered shape matches no cartridge. */
    static uint8_t odd[ISSD_RETAIL_SIZE + 1];
    why[0] = 0;
    assert(!issd_cartridge_stash_retail(odd, sizeof odd, why, sizeof why));
    assert(why[0] != 0);
    assert(strstr(why, "2,097,152") != NULL);

    /* A refused stash leaves nothing behind to patch. */
    assert(issd_cartridge_retail(&size) == NULL && size == 0);

    why[0] = 0;
    assert(!issd_cartridge_stash_retail(NULL, ISSD_RETAIL_SIZE, why, sizeof why));
    assert(why[0] != 0);
}

/* --- IPS apply ----------------------------------------------------------- */

static void test_ips_apply(void) {
    size_t base_size = 0;
    uint8_t *retail = load_retail(&base_size);
    assert(retail != NULL && base_size == ISSD_RETAIL_SIZE);
    assert(issd_cartridge_stash_retail(retail, base_size, NULL, 0));

    static uint8_t out[ISSD_RETAIL_SIZE];
    static uint8_t zero[ISSD_RETAIL_SIZE];
    Patch p;
    char why[256];

    /* The base verification ruling: a buffer that is not the retail image is
     * refused before any byte is written, even when the IPS itself is valid. */
    assert(issd_cartridge_stash_retail(zero, sizeof zero, NULL, 0));
    ips_begin(&p);
    ips_record(&p, 0x1000, "HELLO", 5);
    ips_eof(&p);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "original") != NULL);

    assert(issd_cartridge_stash_retail(retail, base_size, NULL, 0));

    /* Two literal records and one RLE run, then EOF. */
    ips_begin(&p);
    ips_record(&p, 0x1000, "HELLO", 5);
    ips_record(&p, 0x2000, "\xDE\xAD\xBE", 3);
    ips_rle(&p, 0x3000, 4, 0x5A);
    ips_eof(&p);
    why[0] = 0;
    assert(issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                      why, sizeof why));
    assert(why[0] == 0);
    assert(memcmp(out + 0x1000, "HELLO", 5) == 0);
    assert(out[0x1005] == retail[0x1005]);
    assert(out[0x2000] == 0xDE && out[0x2001] == 0xAD && out[0x2002] == 0xBE);
    assert(out[0x3000] == 0x5A && out[0x3003] == 0x5A);
    assert(out[0x3004] == retail[0x3004]);
    assert(out[0] == retail[0] &&
           out[ISSD_RETAIL_SIZE - 1] == retail[ISSD_RETAIL_SIZE - 1]);

    /* Missing PATCH signature. */
    Patch bad = p;
    bad.bytes[0] = 'X';
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(bad.bytes, bad.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "PATCH") != NULL);

    /* A record at the image boundary would expand the cartridge. */
    ips_begin(&p);
    ips_record(&p, ISSD_RETAIL_SIZE, "X", 1);
    ips_eof(&p);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "outside") != NULL);

    /* A record that starts inside but runs past the end is also refused. */
    ips_begin(&p);
    ips_record(&p, ISSD_RETAIL_SIZE - 1, "XY", 2);
    ips_eof(&p);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "outside") != NULL);

    /* IPS truncation extension: three bytes after EOF mean "cut the file
     * here", which would leave a short cartridge. Refused, with a warning. */
    ips_begin(&p);
    ips_eof(&p);
    put_be24(&p, 0x001E84);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "truncat") != NULL);

    /* A literal record whose payload stops early. */
    ips_begin(&p);
    put_be24(&p, 0x1000);
    put_be16(&p, 8);
    put_bytes(&p, "abc", 3);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "truncated") != NULL);

    /* An RLE run with no fill byte. */
    ips_begin(&p);
    put_be24(&p, 0x1000);
    put_be16(&p, 0);
    put_be16(&p, 4);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "truncated") != NULL);

    /* No EOF marker at all. */
    ips_begin(&p);
    ips_record(&p, 0x1000, "HI", 2);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(strstr(why, "EOF") != NULL);

    /* An output buffer smaller than the cartridge is refused outright. */
    ips_begin(&p);
    ips_eof(&p);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out,
                                       ISSD_RETAIL_SIZE - 1, why, sizeof why));
    assert(why[0] != 0);

    /* A refused patch must not leave a half-patched buffer behind. The first
     * record is valid, so a parser that writes as it walks would already have
     * changed the sentinel by the time the short second record is seen. */
    static uint8_t sentinel[ISSD_RETAIL_SIZE];
    memset(sentinel, 0xCC, sizeof sentinel);
    ips_begin(&p);
    ips_record(&p, 0x1000, "OK", 2);
    put_be24(&p, 0x2000);
    put_be16(&p, 8);
    put_bytes(&p, "abc", 3);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, sentinel,
                                       sizeof sentinel, why, sizeof why));
    for (size_t i = 0; i < sizeof sentinel; i++)
        assert(sentinel[i] == 0xCC);

    /* Patching into the retained base itself would corrupt the very image a
     * refused patch is supposed to leave alone, so the alias is refused. */
    ips_begin(&p);
    ips_record(&p, 0x1000, "OK", 2);
    ips_eof(&p);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, (uint8_t *)retail,
                                       sizeof out, why, sizeof why));
    assert(strstr(why, "overlaps") != NULL);
    assert(issd_cartridge_retail(&base_size) == retail);
    assert(base_size == ISSD_RETAIL_SIZE);
    assert(issd_cartridge_sha256_matches(retail, base_size, ISSD_RETAIL_SHA256));
    /* The stash is still usable for a separate output buffer. */
    why[0] = 0;
    assert(issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                      why, sizeof why));

    /* Clearing the stash leaves no base to patch. */
    why[0] = 0;
    assert(!issd_cartridge_stash_retail(NULL, 0, why, sizeof why));
    assert(issd_cartridge_retail(NULL) == NULL);
    ips_begin(&p);
    ips_eof(&p);
    why[0] = 0;
    assert(!issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                       why, sizeof why));
    assert(why[0] != 0);

    free(retail);
}

/* --- image validation ---------------------------------------------------- */

static void test_image_validation(void) {
    size_t size = 0;
    uint8_t *retail = load_retail(&size);
    assert(retail != NULL);
    char why[256];

    /* The header the real cartridge carries: mapping 0x30 (LoROM), type 0x00. */
    why[0] = 0;
    assert(issd_cartridge_validate_image(retail, size, why, sizeof why));
    assert(why[0] == 0);

    static uint8_t image[ISSD_RETAIL_SIZE];
    memset(image, 0, sizeof image);
    image[0x7FD5] = 0x30;
    image[0x7FD6] = 0x00;
    assert(issd_cartridge_validate_image(image, sizeof image, why, sizeof why));

    /* HiROM sets bit 0 of the mapping byte. */
    image[0x7FD5] = 0x31;
    why[0] = 0;
    assert(!issd_cartridge_validate_image(image, sizeof image, why, sizeof why));
    assert(strstr(why, "HiROM") != NULL);

    /* Anything but 0x00 claims a coprocessor. */
    image[0x7FD5] = 0x30;
    image[0x7FD6] = 0x02;
    why[0] = 0;
    assert(!issd_cartridge_validate_image(image, sizeof image, why, sizeof why));
    assert(strstr(why, "coprocessor") != NULL);

    /* Only a headerless 2 MB image is accepted here; stash normalizes the
     * copier-header shape, validation never does. */
    why[0] = 0;
    assert(!issd_cartridge_validate_image(image, sizeof image - 1,
                                          why, sizeof why));
    assert(strstr(why, "2,097,152") != NULL);
    assert(!issd_cartridge_validate_image(NULL, sizeof image,
                                          why, sizeof why));

    free(retail);
}

/* --- SHA-256 helper ------------------------------------------------------ */

static void test_sha256(void) {
    size_t size = 0;
    uint8_t *retail = load_retail(&size);
    assert(retail != NULL);

    assert(issd_cartridge_sha256_matches(retail, size, ISSD_RETAIL_SHA256));

    /* Hexadecimal hashes are written both ways in the wild. */
    char upper[65];
    for (int i = 0; i < 64; i++) {
        char c = ISSD_RETAIL_SHA256[i];
        upper[i] = (char)(c >= 'a' && c <= 'f' ? c - 'a' + 'A' : c);
    }
    upper[64] = 0;
    assert(issd_cartridge_sha256_matches(retail, size, upper));

    /* One doctored byte changes the digest. */
    uint8_t *doctored = malloc(size);
    assert(doctored != NULL);
    memcpy(doctored, retail, size);
    doctored[0x12345] ^= 0xFF;
    assert(!issd_cartridge_sha256_matches(doctored, size, ISSD_RETAIL_SHA256));

    /* Malformed expectations are a miss, not a match. */
    assert(!issd_cartridge_sha256_matches(retail, size, NULL));
    assert(!issd_cartridge_sha256_matches(NULL, size, ISSD_RETAIL_SHA256));
    char short_hash[17];
    memcpy(short_hash, ISSD_RETAIL_SHA256, 16);
    short_hash[16] = 0;
    assert(!issd_cartridge_sha256_matches(retail, size, short_hash));

    free(doctored);
    free(retail);
}

/* --- pack JSON: the cartridge key and the stack resolver ---------------- */

/* Packs are JSON files, so the parser is exercised through a real file rather
 * than by reaching into static code. The harness hands over a scratch folder
 * in argv[1]; a pack's graphics and cartridge resolve against the folder its
 * JSON sits in. */
static char g_scratch[512];

static bool write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static int load_pack_text(const char *name, const char *text) {
    char path[600];
    snprintf(path, sizeof path, "%s/%s", g_scratch, name);
    assert(write_text(path, text));
    return issd_mod_load_pack(path);
}

/* A pack that only swaps the image; the sha256 is the one a modder would
 * write next to it. The name is distinct so a stack test can say which won. */
static const char IMAGE_PACK[] =
    "{\n"
    "  \"name\": \"Image Pack\",\n"
    "  \"author\": \"Tester\",\n"
    "  \"teams\": [ { \"team_id\": 0, \"name\": \"Home\" } ],\n"
    "  \"cartridge\": {\n"
    "    \"image\": \"hack.sfc\",\n"
    "    \"sha256\": \"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\"\n"
    "  }\n"
    "}\n";

static const char PATCH_PACK[] =
    "{\n"
    "  \"name\": \"Patch Pack\",\n"
    "  \"teams\": [ { \"team_id\": 1, \"name\": \"Away\" } ],\n"
    "  \"cartridge\": { \"patch\": \"hack.ips\" }\n"
    "}\n";

/* The pinned schema example carries all three members. A pack may not use
 * both sources at once, so parsing keeps them and the resolver refuses. */
static const char BOTH_PACK[] =
    "{\n"
    "  \"name\": \"Both Pack\",\n"
    "  \"teams\": [ { \"team_id\": 2, \"name\": \"Both\" } ],\n"
    "  \"cartridge\": {\n"
    "    \"image\": \"hack.sfc\",\n"
    "    \"patch\": \"hack.ips\",\n"
    "    \"sha256\": \"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF\"\n"
    "  }\n"
    "}\n";

static const char PLAIN_PACK[] =
    "{\n"
    "  \"name\": \"Plain Pack\",\n"
    "  \"teams\": [ { \"team_id\": 3, \"name\": \"Plain\" } ]\n"
    "}\n";

static const char EMPTY_IMAGE_PACK[] =
    "{\n"
    "  \"name\": \"Empty Pack\",\n"
    "  \"teams\": [ { \"team_id\": 4, \"name\": \"Empty\" } ],\n"
    "  \"cartridge\": { \"image\": \"\" }\n"
    "}\n";

/* A pack may carry nothing but a cartridge; that is a whole feature, not a
 * pack that "changes nothing". */
static const char CARTRIDGE_ONLY_PACK[] =
    "{\n"
    "  \"name\": \"Hack Only\",\n"
    "  \"cartridge\": { \"image\": \"hack.sfc\" }\n"
    "}\n";

static bool resolve(char *name, size_t name_cap, char *file, size_t file_cap,
                    bool *is_patch, char *sha, size_t sha_cap) {
    return issd_mod_cartridge_resolve(name, name_cap, file, file_cap, is_patch,
                                      sha, sha_cap);
}

static void test_cartridge_pack(void) {
    char name[64], file[512], sha[73];
    bool is_patch = true;
    char want[600];

    /* --- the cartridge object parses; nothing happens while it is off ---- */
    assert(issd_mod_init());
    const int image = load_pack_text("image.json", IMAGE_PACK);
    assert(image >= 0);
    const IssdModPack *p = issd_mod_get_pack(image);
    assert(p && strcmp(p->name, "Image Pack") == 0);
    assert(strcmp(p->cartridge_image, "hack.sfc") == 0);
    assert(p->cartridge_patch[0] == 0);
    assert(strcmp(p->cartridge_sha256,
                  "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789") == 0);

    name[0] = file[0] = sha[0] = 'X';
    is_patch = true;
    assert(!resolve(name, sizeof name, file, sizeof file, &is_patch,
                    sha, sizeof sha));
    assert(name[0] == 0 && file[0] == 0 && sha[0] == 0 &&
           "a resolver miss names nothing");

    /* Enabled, it is the cartridge the stack asks for, and the file is named
     * against the pack's own folder so a pack stays one movable folder. */
    issd_mod_set_pack_enabled(image, true);
    assert(resolve(name, sizeof name, file, sizeof file, &is_patch,
                   sha, sizeof sha));
    assert(strcmp(name, "Image Pack") == 0);
    snprintf(want, sizeof want, "%s/hack.sfc", g_scratch);
    assert(strcmp(file, want) == 0);
    assert(!is_patch);
    assert(strcmp(sha,
                  "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789") == 0);
    assert(issd_mod_last_result()->errors == 0);

    /* --- both sources: kept as written, refused by the resolver ---------- */
    assert(issd_mod_init());
    const int both = load_pack_text("both.json", BOTH_PACK);
    assert(both >= 0);
    p = issd_mod_get_pack(both);
    assert(p && strcmp(p->cartridge_image, "hack.sfc") == 0);
    assert(strcmp(p->cartridge_patch, "hack.ips") == 0);
    assert(strcmp(p->cartridge_sha256,
                  "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF") == 0);

    issd_mod_set_pack_enabled(both, true);
    assert(!resolve(name, sizeof name, file, sizeof file, &is_patch,
                    sha, sizeof sha));
    const IssdModResult *r = issd_mod_last_result();
    assert(r->errors == 1 && "an unusable cartridge is an error, not a warning");
    assert(strstr(r->detail, "both") != NULL);
    assert(r->warnings == 0);

    /* --- two enabled cartridge packs: the topmost wins, the other is named */
    assert(issd_mod_init());
    const int lower = load_pack_text("lower.json", IMAGE_PACK);
    const int upper = load_pack_text("upper.json", PATCH_PACK);
    assert(lower >= 0 && upper >= 0);
    issd_mod_set_pack_enabled(lower, true);
    issd_mod_set_pack_enabled(upper, true);

    assert(resolve(name, sizeof name, file, sizeof file, &is_patch,
                   sha, sizeof sha));
    assert(strcmp(name, "Patch Pack") == 0);
    snprintf(want, sizeof want, "%s/hack.ips", g_scratch);
    assert(strcmp(file, want) == 0);
    assert(is_patch);
    assert(sha[0] == 0 && "a pack without a hash resolves with an empty one");
    r = issd_mod_last_result();
    assert(r->warnings == 1);
    assert(strstr(r->detail, "shadow") != NULL &&
           strstr(r->detail, "Image Pack") != NULL);

    /* Enabling one again puts it on top; the shadow follows the stack. */
    issd_mod_result_reset();
    issd_mod_set_pack_enabled(lower, false);
    issd_mod_set_pack_enabled(lower, true);
    assert(resolve(name, sizeof name, file, sizeof file, &is_patch,
                   sha, sizeof sha));
    assert(strcmp(name, "Image Pack") == 0);
    snprintf(want, sizeof want, "%s/hack.sfc", g_scratch);
    assert(strcmp(file, want) == 0);
    assert(!is_patch);
    assert(issd_mod_last_result()->warnings == 1);

    /* --- a cartridge-only pack is a pack, not an empty one --------------- */
    assert(issd_mod_init());
    const int only = load_pack_text("only.json", CARTRIDGE_ONLY_PACK);
    assert(only >= 0);
    r = issd_mod_last_result();
    assert(r->errors == 0 && "a cartridge pack changes something");
    issd_mod_set_pack_enabled(only, true);
    assert(resolve(name, sizeof name, file, sizeof file, &is_patch,
                   sha, sizeof sha));
    assert(strcmp(name, "Hack Only") == 0);
    snprintf(want, sizeof want, "%s/hack.sfc", g_scratch);
    assert(strcmp(file, want) == 0);
    assert(!is_patch);

    /* --- no cartridge anywhere: the retail ROM plays, nothing is noted ---- */
    assert(issd_mod_init());
    const int plain = load_pack_text("plain.json", PLAIN_PACK);
    const int empty = load_pack_text("empty.json", EMPTY_IMAGE_PACK);
    assert(plain >= 0 && empty >= 0);
    issd_mod_set_pack_enabled(plain, true);
    issd_mod_set_pack_enabled(empty, true);
    name[0] = file[0] = sha[0] = 'X';
    assert(!resolve(name, sizeof name, file, sizeof file, &is_patch,
                    sha, sizeof sha));
    assert(name[0] == 0 && file[0] == 0 && sha[0] == 0);
    r = issd_mod_last_result();
    assert(r->errors == 0 && r->warnings == 0);
}

int main(int argc, char **argv) {
    assert(argc > 1 && "the harness must pass a scratch folder");
    snprintf(g_scratch, sizeof g_scratch, "%s", argv[1]);

    test_retail_stash();
    test_ips_apply();
    test_image_validation();
    test_sha256();
    test_cartridge_pack();
    puts("cartridge tests passed");
    return 0;
}
