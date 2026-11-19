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

#define RETAIL_PATH "International Superstar Soccer Deluxe (USA).sfc"
#define COPIER_HEADER 512u

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

int main(void) {
    test_retail_stash();
    test_ips_apply();
    test_image_validation();
    test_sha256();
    puts("cartridge tests passed");
    return 0;
}
