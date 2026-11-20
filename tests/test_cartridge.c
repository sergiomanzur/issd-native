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
#include "sha256.h"

#define RETAIL_PATH "International Superstar Soccer Deluxe (USA).sfc"
#define COPIER_HEADER 512u

/* The cartridge's own first squad, which an added team's roster is copied
 * from. Matches ROM_NAME_BASE in issd_mod_rom.c. */
#define ROM_NAME_BASE 229774u

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
    assert(base != NULL && base != headered + COPIER_HEADER &&
           "the module keeps its own copy, not a pointer into the caller's "
           "buffer");
    assert(size == ISSD_RETAIL_SIZE);
    assert(base[0] == 0x11 && base[ISSD_RETAIL_SIZE - 1] == 0x11);

    static uint8_t plain[ISSD_RETAIL_SIZE];
    memset(plain, 0x22, sizeof plain);
    assert(issd_cartridge_stash_retail(plain, sizeof plain, why, sizeof why));
    base = issd_cartridge_retail(&size);
    assert(base != plain && size == ISSD_RETAIL_SIZE && base[0] == 0x22);

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
     * refused patch is supposed to leave alone, so the alias is refused. The
     * retained base is the module's own copy, so only that pointer aliases. */
    {
        size_t retained_size = 0;
        uint8_t *retained =
            (uint8_t *)issd_cartridge_retail(&retained_size);
        assert(retained != NULL && retained_size == ISSD_RETAIL_SIZE);
        ips_begin(&p);
        ips_record(&p, 0x1000, "OK", 2);
        ips_eof(&p);
        why[0] = 0;
        assert(!issd_cartridge_apply_patch(p.bytes, p.len, retained,
                                           retained_size, why, sizeof why));
        assert(strstr(why, "overlaps") != NULL);
        assert(issd_cartridge_retail(&base_size) == retained);
        assert(base_size == ISSD_RETAIL_SIZE);
        assert(issd_cartridge_sha256_matches(retained, base_size,
                                             ISSD_RETAIL_SHA256));
    }
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

/* --- the stash is a copy, so mod writes cannot poison it ------------------ */

/* The mod engine writes packs into the buffer main loaded and plays. If the
 * module borrowed that buffer, every roster pack would poison the IPS base and
 * an IPS cartridge pack could never be enabled in-process: the regression is
 * roster pack active, no cartridge pack, then enable an IPS pack and restart. */
static void test_stash_is_a_copy(void) {
    size_t retail_size = 0;
    uint8_t *retail = load_retail(&retail_size);
    assert(retail != NULL && retail_size == ISSD_RETAIL_SIZE);
    assert(issd_cartridge_stash_retail(retail, retail_size, NULL, 0));

    /* What issd_mod_reapply does to the image the game plays. */
    memset(retail + 0x1000, 0x00, 64);
    memset(retail + 0x2000, 0xEE, 32);

    size_t kept = 0;
    const uint8_t *base = issd_cartridge_retail(&kept);
    assert(base != NULL && base != retail && kept == ISSD_RETAIL_SIZE);
    assert(issd_cartridge_sha256_matches(base, kept, ISSD_RETAIL_SHA256) &&
           "pack writes to the caller's buffer must not reach the stash");

    Patch p;
    ips_begin(&p);
    ips_record(&p, 0x3000, "OK", 2);
    ips_eof(&p);
    static uint8_t out[ISSD_RETAIL_SIZE];
    char why[128];
    why[0] = 0;
    assert(issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                      why, sizeof why));
    assert(memcmp(out + 0x1000, base + 0x1000, 64) == 0 &&
           "the patch starts from the clean retail image");
    assert(out[0x1000] != 0x00 && out[0x2000] != 0xEE);

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

/* --- added rosters over a hacked cartridge ------------------------------- */

/* Adding a team writes a squad into the free run at the tail of bank $87
 * ($87:FC40). A cartridge pack may be a hack that keeps its own data there,
 * so the write must refuse and warn instead of clobbering it (design ruling
 * 5). The run is blank in the retail cartridge - $FF everywhere - and some
 * images zero-fill free space, so both fills count as free. */
static void test_added_roster_free_space(void) {
    const size_t GATE_A = 0x004F2Eu, GATE_B = 0x004F50u;
    const size_t PTRS = 0x038138u;
    const size_t ROSTERS = 0x03FC40u;
    const size_t RUN = (size_t)ISSD_MAX_ADDED_TEAMS * 160u;

    assert(issd_mod_init());
    const int pi = load_pack_text("added.json", PLAIN_PACK);
    assert(pi >= 0);
    issd_mod_set_pack_enabled(pi, true);
    IssdModPack *pack = issd_mod_get_pack(pi);
    assert(pack);
    pack->team_count = 1;
    memset(&pack->teams[0], 0, sizeof pack->teams[0]);
    pack->teams[0].new_team = true;
    pack->teams[0].assigned_slot = -1;
    pack->teams[0].kit_record = -1;
    snprintf(pack->teams[0].name, sizeof pack->teams[0].name, "Hack Town");

    static uint8_t image[ISSD_RETAIL_SIZE];
    memset(image, 0xEE, sizeof image);
    /* The bytes the measured revision carries: the squad loader's gate and
     * the roster pointer table's first entry. */
    image[GATE_A - 1] = 0xE0; image[GATE_A] = 0x48; image[GATE_A + 1] = 0x00;
    image[GATE_B - 1] = 0xE0; image[GATE_B] = 0x48; image[GATE_B + 1] = 0x00;
    image[PTRS] = 0x8E; image[PTRS + 1] = 0x81;
    /* A recognisable source squad, so the copied roster is checkable. */
    image[ROM_NAME_BASE + 5] = 0x77;

    /* --- a hack occupies the run: refuse, warn, change nothing ----------- */
    memset(image + ROSTERS, 0x01, RUN);          /* not a blank fill */
    issd_mod_result_reset();
    issd_mod_apply_to_rom(image, sizeof image);

    assert(issd_mod_last_result()->warnings > 0 &&
           "an occupied run must be reported");
    assert(pack->teams[0].assigned_slot < 0 &&
           "no slot is handed out when its roster cannot be written");
    assert(image[GATE_A] == 0x48 && image[GATE_A + 1] == 0x00 &&
           "the gate is not raised without rosters behind it");
    assert((uint16_t)(image[PTRS + 36 * 2] | (image[PTRS + 36 * 2 + 1] << 8))
           == 0xEEEEu && "nor is the pointer table re-pointed");
    for (size_t i = 0; i < RUN; i++)
        assert(image[ROSTERS + i] == 0x01 &&
               "the hack's own bytes are left exactly as they were");

    /* --- an all-zero run is NOT free ------------------------------------- */
    /* Ruling 5 mandates the same free-space rule as rom_is_free: only $FF is
     * blank. A rebuilt hack may keep meaningful zero bytes there, so refusing
     * is the safe direction - the pack simply gets no added team. */
    memset(image + ROSTERS, 0x00, RUN);
    issd_mod_result_reset();
    issd_mod_apply_to_rom(image, sizeof image);
    assert(issd_mod_last_result()->warnings > 0 &&
           "a zero-filled run must be reported as occupied");
    assert(pack->teams[0].assigned_slot < 0);
    assert(image[GATE_A] == 0x48 && image[GATE_A + 1] == 0x00 &&
           "the gate is not raised without rosters behind it");
    for (size_t i = 0; i < RUN; i++)
        assert(image[ROSTERS + i] == 0x00 &&
               "the zero bytes are left exactly as they were");

    /* --- the cartridge's $FF fill is free -------------------------------- */
    /* A fresh image: the stack is re-applied from the pristine copy at
     * runtime, so the gate is back to its retail operand here. */
    image[GATE_A] = 0x48; image[GATE_A + 1] = 0x00;
    image[GATE_B] = 0x48; image[GATE_B + 1] = 0x00;
    memset(image + ROSTERS, 0xFF, RUN);
    issd_mod_result_reset();
    issd_mod_apply_to_rom(image, sizeof image);
    assert(pack->teams[0].assigned_slot == 36);
    assert((uint16_t)(image[GATE_A] | (image[GATE_A + 1] << 8)) == 37 * 2);
    assert(image[ROSTERS + 5] == 0x77 &&
           "the added squad is a copy of the cartridge's own squad 0");
    assert((uint16_t)(image[PTRS + 36 * 2] | (image[PTRS + 36 * 2 + 1] << 8))
           == (uint16_t)(0x8000u + (ROSTERS - 0x38000u)));
}

/* --- the effective base, rebuilt at boot and after a restart -------------- */

static bool write_bytes(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    const bool ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok;
}

static void hex_digest(const void *data, size_t size, char out[65]) {
    uint8_t digest[32];
    sha256_compute((const uint8_t *)data, size, digest);
    for (int i = 0; i < 32; i++)
        snprintf(out + i * 2, 3, "%02x", digest[i]);
}

/* --- the engine's image is a work copy of the immutable stash ------------ */

/* Fix round 2 regression: select_base must never hand out the stash itself.
 * The engine is given a work copy; pack writes land there, a later select
 * refreshes it, and both the stash and every IPS apply stay pristine. This is
 * the boot-with-no-pack path and the restart-to-retail path. */
static void test_retail_work_copy(void) {
    size_t retail_size = 0;
    uint8_t *retail = load_retail(&retail_size);
    assert(retail != NULL && retail_size == ISSD_RETAIL_SIZE);
    assert(issd_cartridge_stash_retail(retail, retail_size, NULL, 0));

    size_t stash_size = 0;
    const uint8_t *stash = issd_cartridge_retail(&stash_size);
    assert(stash != NULL && stash != retail &&
           stash_size == ISSD_RETAIL_SIZE);

    size_t size = 0;
    bool changed = true;
    char why[256];

    /* Boot with no cartridge pack: the engine plays a copy, not the stash. */
    why[0] = 'X';
    const uint8_t *work = issd_cartridge_select_base(
        NULL, false, "", &size, &changed, why, sizeof why);
    assert(work != NULL && work != retail && work != stash &&
           "the selected retail base is a work copy, never the stash");
    assert(size == ISSD_RETAIL_SIZE && why[0] == 0);
    assert(memcmp(work, stash, ISSD_RETAIL_SIZE) == 0);
    assert(!changed);

    /* What issd_mod_reapply does: write pack bytes into the live image. */
    memset((uint8_t *)work + 0x1000, 0x00, 64);
    memset((uint8_t *)work + 0x2000, 0xEE, 32);

    /* The stash does not move and stays retail. */
    size_t kept = 0;
    assert(issd_cartridge_retail(&kept) == stash && kept == stash_size);
    assert(issd_cartridge_sha256_matches(stash, kept, ISSD_RETAIL_SHA256) &&
           "pack writes to the live image must never reach the stash");

    /* An IPS pack enabled now must still find a verified retail base. */
    Patch p;
    ips_begin(&p);
    ips_record(&p, 0x3000, "OK", 2);
    ips_eof(&p);
    static uint8_t out[ISSD_RETAIL_SIZE];
    why[0] = 0;
    assert(issd_cartridge_apply_patch(p.bytes, p.len, out, sizeof out,
                                      why, sizeof why));
    assert(memcmp(out + 0x1000, stash + 0x1000, 64) == 0);
    assert(out[0x1000] != 0x00 && out[0x2000] != 0xEE);

    /* Restart to retail: the same work buffer comes back refreshed, and the
     * caller is told to re-apply because the refresh discarded pack writes. */
    const uint8_t *again = issd_cartridge_select_base(
        NULL, false, "", &size, &changed, why, sizeof why);
    assert(again == work && size == ISSD_RETAIL_SIZE);
    assert(changed && "a refreshed live image must be re-applied");
    assert(memcmp(again + 0x1000, stash + 0x1000, 64) == 0 &&
           again[0x1000] != 0x00);
    assert(memcmp(again + 0x2000, stash + 0x2000, 32) == 0);

    /* A pack image, then back to retail, also refreshes the work copy. */
    static uint8_t image[ISSD_RETAIL_SIZE];
    memset(image, 0x11, sizeof image);
    image[0x7FD5] = 0x30;
    image[0x7FD6] = 0x00;
    char path[600];
    snprintf(path, sizeof path, "%s/work.sfc", g_scratch);
    assert(write_bytes(path, image, sizeof image));
    const uint8_t *hack = issd_cartridge_select_base(
        path, false, "", &size, &changed, why, sizeof why);
    assert(hack != NULL && hack != work && hack != stash && changed);
    memset((uint8_t *)hack + 0x1000, 0x55, 16);

    const uint8_t *back = issd_cartridge_select_base(
        NULL, false, "", &size, &changed, why, sizeof why);
    assert(back == work && changed && size == ISSD_RETAIL_SIZE);
    assert(memcmp(back + 0x1000, stash + 0x1000, 64) == 0);
    assert(issd_cartridge_sha256_matches(stash, kept, ISSD_RETAIL_SHA256));

    free(retail);
}

/* A restart changes the enabled stack without reloading the ROM file, so the
 * base has to be rebuilt from the retained retail image - and only when the
 * winner actually changed. This is the function both the boot path and the
 * in-process restart call. */
static void test_select_base(void) {
    size_t retail_size = 0;
    uint8_t *retail = load_retail(&retail_size);
    assert(retail != NULL && retail_size == ISSD_RETAIL_SIZE);
    assert(issd_cartridge_stash_retail(retail, retail_size, NULL, 0));
    /* The module keeps a private copy; its base is not the caller's buffer. */
    const uint8_t *retained = issd_cartridge_retail(NULL);
    assert(retained != NULL && retained != retail);

    static uint8_t image[ISSD_RETAIL_SIZE];
    memset(image, 0x11, sizeof image);
    image[0x7FD5] = 0x30;      /* LoROM */
    image[0x7FD6] = 0x00;      /* ROM only */
    image[0x1000] = 0x77;

    char path[600], why[256];
    snprintf(path, sizeof path, "%s/hack.sfc", g_scratch);
    assert(write_bytes(path, image, sizeof image));

    size_t size = 0;
    bool changed = true;

    /* No pack: the engine is handed a work copy of the retained retail image
     * and nothing changed. */
    why[0] = 'X';
    const uint8_t *base = issd_cartridge_select_base(
        NULL, false, "", &size, &changed, why, sizeof why);
    const uint8_t *work = base;
    assert(base == work && base != retained && size == ISSD_RETAIL_SIZE);
    assert(!changed && "the retail image already in play is not a change");
    assert(why[0] == 0);

    /* An image pack becomes the base. */
    base = issd_cartridge_select_base(path, false, "", &size, &changed,
                                      why, sizeof why);
    assert(base != NULL && base != retained && size == ISSD_RETAIL_SIZE);
    assert(changed && why[0] == 0);
    assert(memcmp(base, image, sizeof image) == 0);

    /* Asking for the same file again keeps the very same buffer: a restart
     * that changed nothing must not re-read two megabytes. */
    const uint8_t *again = issd_cartridge_select_base(path, false, "", &size,
                                                      &changed, why,
                                                      sizeof why);
    assert(again == base && !changed);

    /* The optional hash is checked when the pack carries one, and a mismatch
     * puts the retail cartridge back rather than playing an unknown image. */
    char zero_hash[65];
    memset(zero_hash, '0', 64);
    zero_hash[64] = 0;
    again = issd_cartridge_select_base(path, false, zero_hash, &size, &changed,
                                       why, sizeof why);
    assert(again == work && changed && size == ISSD_RETAIL_SIZE);
    assert(why[0] != 0 && strstr(why, "sha256") != NULL);

    /* An image the engine cannot run is refused before the hash matters. The
     * refusal refreshes and returns the retail work copy, so it is a change:
     * the refresh discarded whatever the previous reapply wrote there. */
    static uint8_t hirom[ISSD_RETAIL_SIZE];
    memcpy(hirom, image, sizeof hirom);
    hirom[0x7FD5] = 0x31;
    char hirom_path[600];
    snprintf(hirom_path, sizeof hirom_path, "%s/hirom.sfc", g_scratch);
    assert(write_bytes(hirom_path, hirom, sizeof hirom));
    again = issd_cartridge_select_base(hirom_path, false, "", &size, &changed,
                                       why, sizeof why);
    assert(again == work && size == ISSD_RETAIL_SIZE);
    assert(changed && "a refreshed retail work copy must be re-applied");
    assert(why[0] != 0 && strstr(why, "HiROM") != NULL);

    /* The matching hash is accepted. */
    char digest[65];
    hex_digest(image, sizeof image, digest);
    base = issd_cartridge_select_base(path, false, digest, &size, &changed,
                                      why, sizeof why);
    assert(base != NULL && base != work && changed && why[0] == 0);
    assert(memcmp(base + 0x1000, image + 0x1000, 1) == 0);

    /* A patch applies to the retained retail image, not to the pack that was
     * in play before it. */
    Patch p;
    ips_begin(&p);
    ips_record(&p, 0x2000, "PATCHED", 7);
    ips_eof(&p);
    char ips[600];
    snprintf(ips, sizeof ips, "%s/hack.ips", g_scratch);
    assert(write_bytes(ips, p.bytes, p.len));
    const uint8_t *patched = issd_cartridge_select_base(ips, true, "", &size,
                                                        &changed, why,
                                                        sizeof why);
    assert(patched != NULL && patched != work && changed && why[0] == 0);
    assert(size == ISSD_RETAIL_SIZE);
    assert(memcmp(patched + 0x2000, "PATCHED", 7) == 0);
    assert(memcmp(patched + 0x1000, retained + 0x1000, 4) == 0 &&
           "an IPS patch starts from retail, never from the previous hack");

    /* Switching the pack off returns to the retail work copy. */
    const uint8_t *back = issd_cartridge_select_base(NULL, false, "", &size,
                                                     &changed, why, sizeof why);
    assert(back == work && changed && size == ISSD_RETAIL_SIZE);

    /* A file that is not there is refused with a reason and retail stays. */
    snprintf(path, sizeof path, "%s/missing.sfc", g_scratch);
    why[0] = 0;
    back = issd_cartridge_select_base(path, false, "", &size, &changed, why,
                                      sizeof why);
    assert(back == work && changed);
    assert(why[0] != 0);

    free(retail);
}

int main(int argc, char **argv) {
    assert(argc > 1 && "the harness must pass a scratch folder");
    snprintf(g_scratch, sizeof g_scratch, "%s", argv[1]);

    test_retail_stash();
    test_ips_apply();
    test_stash_is_a_copy();
    test_retail_work_copy();
    test_image_validation();
    test_sha256();
    test_cartridge_pack();
    test_added_roster_free_space();
    test_select_base();

    /* The exit path hands the module's images back; release is idempotent. */
    issd_cartridge_release();
    assert(issd_cartridge_retail(NULL) == NULL);
    issd_cartridge_release();
    puts("cartridge tests passed");
    return 0;
}
