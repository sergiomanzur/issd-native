#include "protocol/rnet_protocol.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void expect_true(int cond, const char *msg)
{
    if (!cond)
    {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_failures++;
    }
}

#define MAGIC 0x524E4554u

int main(void)
{
    rnet_u8 buf[RNET_MAX_PACKET];
    RNetDecodedPacket dec;
    RNetRbWireFrame rows[3];
    int n;
    int i;

    /* RB_SYNC round-trip (op + flags byte) */
    n = rnet_proto_encode_rb_sync(buf, sizeof(buf), MAGIC, 0xABCD, 1, 7u, 50u, 48u, 56u, 1u, 1u, 0x03u);
    expect_true(n > 0, "rb_sync encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_sync decodes");
    expect_true(dec.type == RNET_PKT_RB_SYNC, "rb_sync type");
    expect_true(dec.session_id == 0xABCD && dec.local_slot == 1, "rb_sync session/slot");
    expect_true(dec.rb_epoch_id == 7u && dec.rb_mismatch_tick == 50u, "rb_sync epoch/mismatch");
    expect_true(dec.rb_load_tick == 48u && dec.rb_target_tick == 56u, "rb_sync load/target");
    expect_true(dec.rb_corrected_slot == 1u && dec.rb_initiator == 1u, "rb_sync slot/initiator");
    expect_true(dec.rb_flags == 0x03u, "rb_sync flags");

    /* RB_SYNC abort op round-trip (class in mismatch field) */
    n = rnet_proto_encode_rb_sync(buf, sizeof(buf), MAGIC, 0xABCD, 1, 9u, 2u, 44u, 0u, 0u, 2u, 0u);
    expect_true(n > 0, "rb_sync abort encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_sync abort decodes");
    expect_true(dec.rb_initiator == 2u && dec.rb_mismatch_tick == 2u && dec.rb_load_tick == 44u,
                "rb_sync abort op/class/realign");

    /* RB_SEAL_ROWS round-trip */
    for (i = 0; i < 3; ++i)
    {
        rows[i].buttons = (rnet_u16)(0x300u + i);
        rows[i].stick_x = (rnet_s8)(20 + i);
        rows[i].stick_y = (rnet_s8)-15;
        rows[i].source = 1u;
        rows[i].is_predicted = 0u;
        rows[i].is_valid = 1u;
    }
    n = rnet_proto_encode_rb_seal_rows(buf, sizeof(buf), MAGIC, 0xABCD, 1, 7u, 50u, 56u, 1u, 0u, rows, 3u);
    expect_true(n > 0, "rb_seal_rows encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_seal_rows decodes");
    expect_true(dec.type == RNET_PKT_RB_SEAL_ROWS, "rb_seal_rows type");
    expect_true(dec.rb_slot == 1u && dec.rb_row_count == 3u && dec.rb_row_begin == 0u, "rb_seal_rows meta");
    expect_true(dec.rb_rows[0].buttons == 0x300u && dec.rb_rows[2].buttons == 0x302u, "rb_seal_rows payload");
    expect_true(dec.rb_rows[1].stick_x == 21 && dec.rb_rows[1].is_valid == 1u, "rb_seal_rows sticks");

    /* Truncated seal rows rejected */
    expect_true(rnet_proto_decode(buf, 12u, MAGIC, &dec) != 0, "truncated packet rejected");

    /* RB_BASELINE round-trip */
    n = rnet_proto_encode_rb_baseline(buf, sizeof(buf), MAGIC, 0xABCD, 0, 7u, 48u, 0x11111111u, 0x22222222u,
                                      0x33333333u, 0x44444444u);
    expect_true(n > 0, "rb_baseline encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_baseline decodes");
    expect_true(dec.type == RNET_PKT_RB_BASELINE, "rb_baseline type");
    expect_true(dec.rb_digest_master == 0x11111111u && dec.rb_digest_c == 0x44444444u, "rb_baseline digests");

    /* RB_POST round-trip */
    n = rnet_proto_encode_rb_post(buf, sizeof(buf), MAGIC, 0xABCD, 0, 7u, 56u, 0xDEADu, 0xBEEFu, 1u);
    expect_true(n > 0, "rb_post encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_post decodes");
    expect_true(dec.type == RNET_PKT_RB_POST, "rb_post type");
    expect_true(dec.rb_epoch_id == 7u && dec.rb_target_tick == 56u, "rb_post tip binding");
    expect_true(dec.rb_digest_master == 0xDEADu && dec.rb_input_digest == 0xBEEFu && dec.rb_match == 1u,
                "rb_post digests/match");

    /* RB_FRAME_COMMIT round-trip */
    n = rnet_proto_encode_rb_frame_commit(buf, sizeof(buf), MAGIC, 0xABCD, 1, 60u, 0xCAFEu);
    expect_true(n > 0, "rb_frame_commit encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_frame_commit decodes");
    expect_true(dec.type == RNET_PKT_RB_FRAME_COMMIT, "rb_frame_commit type");
    expect_true(dec.rb_through_tick == 60u && dec.rb_state_hash == 0xCAFEu, "rb_frame_commit payload");

    /* RB_RESOLVED round-trip */
    n = rnet_proto_encode_rb_resolved(buf, sizeof(buf), MAGIC, 0xABCD, 1, 61u);
    expect_true(n > 0, "rb_resolved encodes");
    expect_true(rnet_proto_decode(buf, (size_t)n, MAGIC, &dec) == 0, "rb_resolved decodes");
    expect_true(dec.type == RNET_PKT_RB_RESOLVED && dec.rb_resolved_through == 61u, "rb_resolved payload");

    /* Bad magic rejected */
    expect_true(rnet_proto_decode(buf, (size_t)n, 0xBADu, &dec) != 0, "bad magic rejected");

    if (g_failures == 0)
    {
        printf("rb_wire_test: ok\n");
        return 0;
    }
    fprintf(stderr, "rb_wire_test: %d failure(s)\n", g_failures);
    return 1;
}
