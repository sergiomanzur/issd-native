#include "recomp_net/address.h"
#include "platform/rnet_stun_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STUN_COOKIE 0x2112a442U

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static void put16(rnet_u8 *out, rnet_u16 value)
{
    out[0] = (rnet_u8)(value >> 8);
    out[1] = (rnet_u8)value;
}

static void put32(rnet_u8 *out, rnet_u32 value)
{
    out[0] = (rnet_u8)(value >> 24);
    out[1] = (rnet_u8)(value >> 16);
    out[2] = (rnet_u8)(value >> 8);
    out[3] = (rnet_u8)value;
}

static size_t response_header(rnet_u8 *packet, const rnet_u8 transaction[12])
{
    memset(packet, 0, 128);
    put16(packet, 0x0101);
    put32(packet + 4, STUN_COOKIE);
    memcpy(packet + 8, transaction, 12);
    return 20;
}

static size_t add_mapped(rnet_u8 *packet, size_t offset, rnet_u16 type,
                         rnet_u32 address)
{
    put16(packet + offset, type);
    put16(packet + offset + 2, 8);
    packet[offset + 4] = 0;
    packet[offset + 5] = 1;
    put16(packet + offset + 6, type == 0x0020 ? (rnet_u16)(7777 ^ 0x2112)
                                              : (rnet_u16)7777);
    put32(packet + offset + 8,
          type == 0x0020 ? address ^ STUN_COOKIE : address);
    return offset + 12;
}

static size_t add_padded_unknown(rnet_u8 *packet, size_t offset)
{
    put16(packet + offset, 0x8022);
    put16(packet + offset + 2, 3);
    packet[offset + 4] = 'x';
    packet[offset + 5] = 'y';
    packet[offset + 6] = 'z';
    packet[offset + 7] = 0; /* Attributes are padded to a 32-bit boundary. */
    return offset + 8;
}

static void finish_response(rnet_u8 *packet, size_t length)
{
    put16(packet + 2, (rnet_u16)(length - 20));
}

static void parser_tests(void)
{
    static const rnet_u8 transaction[12] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb
    };
    rnet_u8 packet[128];
    rnet_u8 other_transaction[12];
    rnet_u32 address = 0;
    rnet_u16 mapped_port = 0;
    size_t length;
    int parsed;

    length = response_header(packet, transaction);
    length = add_padded_unknown(packet, length);
    length = add_mapped(packet, length, 0x0020, 0xcb007109U);
    finish_response(packet, length);
    parsed = rnet_stun_parse_binding_response_ex(packet, length, transaction,
                                                 &address, &mapped_port);
    expect_true(parsed == RNET_STUN_PARSE_OK && address == 0xcb007109U &&
                    mapped_port == 7777U,
                "XOR-MAPPED-ADDRESS decodes RFC cookie + port");

    length = response_header(packet, transaction);
    length = add_mapped(packet, length, 0x0001, 0xc6336407U);
    finish_response(packet, length);
    parsed = rnet_stun_parse_binding_response_ex(packet, length, transaction,
                                                 &address, &mapped_port);
    expect_true(parsed == RNET_STUN_PARSE_OK && address == 0xc6336407U &&
                    mapped_port == 7777U,
                "MAPPED-ADDRESS is accepted as fallback with port");

    length = response_header(packet, transaction);
    length = add_mapped(packet, length, 0x0001, 0xc6336407U);
    length = add_mapped(packet, length, 0x0020, 0xcb007109U);
    finish_response(packet, length);
    parsed = rnet_stun_parse_binding_response(packet, length, transaction,
                                              &address);
    expect_true(parsed == RNET_STUN_PARSE_OK && address == 0xcb007109U,
                "XOR-MAPPED-ADDRESS takes precedence");

    memcpy(other_transaction, transaction, sizeof(other_transaction));
    other_transaction[11] ^= 1;
    expect_true(rnet_stun_parse_binding_response(
                    packet, length, other_transaction, &address) ==
                    RNET_STUN_PARSE_TRANSACTION_MISMATCH,
                "transaction mismatch is distinguished from malformed data");

    packet[4] ^= 1;
    expect_true(rnet_stun_parse_binding_response(packet, length, transaction,
                                                  &address) ==
                    RNET_STUN_PARSE_ERROR,
                "bad magic cookie is rejected");
    packet[4] ^= 1;

    expect_true(rnet_stun_parse_binding_response(packet, length + 1,
                                                  transaction, &address) ==
                    RNET_STUN_PARSE_ERROR,
                "bytes beyond the declared STUN message are rejected");

    put16(packet + 2, 7);
    expect_true(rnet_stun_parse_binding_response(packet, length, transaction,
                                                  &address) ==
                    RNET_STUN_PARSE_ERROR,
                "non-aligned message length is rejected");

    length = response_header(packet, transaction);
    put16(packet + length, 0x0020);
    put16(packet + length + 2, 8);
    length += 8; /* Claims 8 value bytes but supplies only 4. */
    finish_response(packet, length);
    expect_true(rnet_stun_parse_binding_response(packet, length, transaction,
                                                  &address) ==
                    RNET_STUN_PARSE_ERROR,
                "truncated attribute is rejected");

    length = response_header(packet, transaction);
    length = add_mapped(packet, length, 0x0020, 0U);
    finish_response(packet, length);
    expect_true(rnet_stun_parse_binding_response(packet, length, transaction,
                                                  &address) ==
                    RNET_STUN_PARSE_ERROR,
                "unspecified mapped address is rejected");

    length = response_header(packet, transaction);
    finish_response(packet, length);
    expect_true(rnet_stun_parse_binding_response(packet, length, transaction,
                                                  &address) ==
                    RNET_STUN_PARSE_ERROR,
                "success response without a mapped address is rejected");

    expect_true(rnet_stun_parse_binding_response(NULL, 0, transaction,
                                                  &address) ==
                    RNET_STUN_PARSE_ERROR,
                "null packet is rejected safely");
}

static void config_test(void)
{
    RNetExternalIpv4Config config;
    memset(&config, 0, sizeof(config));
    rnet_external_ipv4_config_init(&config);
    expect_true(config.stun_host != NULL && config.stun_host[0] != '\0',
                "default STUN host is populated");
    expect_true(config.stun_port == RNET_STUN_DEFAULT_PORT,
                "default STUN port is populated");
    expect_true(config.timeout_ms == RNET_STUN_DEFAULT_TIMEOUT_MS,
                "default STUN timeout is populated");
    expect_true(RNET_STUN_DEFAULT_TIMEOUT_MS <= RNET_STUN_MAX_TIMEOUT_MS &&
                RNET_STUN_MAX_TIMEOUT_MS <= 1000,
                "synchronous STUN deadline is capped at one second");
    expect_true(rnet_external_ipv4_discover(NULL, NULL, 0) ==
                    RNET_EXTERNAL_IPV4_ERR_ARGUMENT,
                "public discovery rejects a missing output buffer");
}

static void optional_live_test(void)
{
    const char *enabled = getenv("RNET_RUN_LIVE_STUN");
    char address[RNET_IPV4_ADDRESS_TEXT_MAX];
    int result;

    if (enabled == NULL || enabled[0] == '\0' || enabled[0] == '0')
    {
        printf("stun_test: live discovery skipped (set RNET_RUN_LIVE_STUN=1)\n");
        return;
    }
    result = rnet_external_ipv4_discover(NULL, address, sizeof(address));
    expect_true(result == RNET_EXTERNAL_IPV4_OK,
                "live default STUN discovery succeeds");
    if (result == RNET_EXTERNAL_IPV4_OK)
    {
        expect_true(address[0] != '\0', "live external IPv4 is nonempty");
        printf("stun_test: live external IPv4 %s\n", address);
    }
}

int main(void)
{
    parser_tests();
    config_test();
    optional_live_test();
    if (failures == 0)
    {
        printf("stun_test: ok\n");
        return 0;
    }
    fprintf(stderr, "stun_test: %d failure(s)\n", failures);
    return 1;
}
