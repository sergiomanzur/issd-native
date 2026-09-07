#include "rnet_protocol.h"

#include <string.h>

static void write_u16(rnet_u8 **cursor, rnet_u16 v)
{
    (*cursor)[0] = (rnet_u8)(v & 0xFFu);
    (*cursor)[1] = (rnet_u8)((v >> 8) & 0xFFu);
    *cursor += 2;
}

static void write_u32(rnet_u8 **cursor, rnet_u32 v)
{
    (*cursor)[0] = (rnet_u8)(v & 0xFFu);
    (*cursor)[1] = (rnet_u8)((v >> 8) & 0xFFu);
    (*cursor)[2] = (rnet_u8)((v >> 16) & 0xFFu);
    (*cursor)[3] = (rnet_u8)((v >> 24) & 0xFFu);
    *cursor += 4;
}

static rnet_u16 read_u16(const rnet_u8 **cursor)
{
    rnet_u16 v = (rnet_u16)(*cursor)[0] | ((rnet_u16)(*cursor)[1] << 8);
    *cursor += 2;
    return v;
}

static rnet_u32 read_u32(const rnet_u8 **cursor)
{
    rnet_u32 v = (rnet_u32)(*cursor)[0] | ((rnet_u32)(*cursor)[1] << 8) | ((rnet_u32)(*cursor)[2] << 16) |
                 ((rnet_u32)(*cursor)[3] << 24);
    *cursor += 4;
    return v;
}

rnet_u32 rnet_proto_checksum(const rnet_u8 *data, size_t len)
{
    rnet_u32 sum = 0x811c9dc5u;
    size_t i;
    for (i = 0; i < len; ++i)
    {
        sum ^= data[i];
        sum *= 0x01000193u;
    }
    return sum;
}

static int finish_packet(rnet_u8 *out, rnet_u8 *cursor, size_t cap)
{
    size_t body = (size_t)(cursor - out);
    rnet_u32 csum;
    if (body + 4 > cap)
    {
        return -1;
    }
    csum = rnet_proto_checksum(out, body);
    write_u32(&cursor, csum);
    return (int)(cursor - out);
}

int rnet_proto_encode_hello(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                            rnet_u8 slot_count, rnet_u8 delay)
{
    rnet_u8 *c = out;
    if (cap < 20)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_HELLO);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = slot_count;
    *c++ = delay;
    *c++ = 0;
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_ready(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot)
{
    rnet_u8 *c = out;
    if (cap < 16)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_READY);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    *c++ = 0;
    *c++ = 0;
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_start(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u32 start_tick)
{
    rnet_u8 *c = out;
    if (cap < 20)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_START);
    write_u32(&c, session_id);
    write_u32(&c, start_tick);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_input(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                            rnet_u16 input_epoch, rnet_u32 ack_tick, const RNetWireFrame *frames,
                            int frame_count)
{
    rnet_u8 *c = out;
    int i;
    if (frame_count < 1 || frame_count > RNET_MAX_BUNDLE || frames == NULL)
    {
        return -1;
    }
    if (cap < 32)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_INPUT);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = (rnet_u8)frame_count;
    *c++ = (rnet_u8)(input_epoch & 0xFFu);
    *c++ = (rnet_u8)((input_epoch >> 8) & 0xFFu);
    write_u32(&c, ack_tick);
    for (i = 0; i < frame_count; ++i)
    {
        rnet_u16 sz = frames[i].size;
        if (sz > RNET_INPUT_MAX)
        {
            return -1;
        }
        if ((size_t)(c - out) + 4 + 2 + sz + 4 > cap)
        {
            return -1;
        }
        write_u32(&c, frames[i].tick);
        write_u16(&c, sz);
        if (sz > 0)
        {
            memcpy(c, frames[i].bytes, sz);
            c += sz;
        }
    }
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_delay_sync(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 new_delay,
                                 rnet_u32 effective_tick)
{
    rnet_u8 *c = out;
    if (cap < 24)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_DELAY_SYNC);
    write_u32(&c, session_id);
    *c++ = new_delay;
    *c++ = 0;
    *c++ = 0;
    *c++ = 0;
    write_u32(&c, effective_tick);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_input_confirm(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                                    rnet_u16 input_epoch, rnet_u32 sim_tick, rnet_u32 input_hash)
{
    rnet_u8 *c = out;
    if (cap < 28)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_INPUT_CONFIRM);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = (rnet_u8)(input_epoch & 0xFFu);
    *c++ = (rnet_u8)((input_epoch >> 8) & 0xFFu);
    *c++ = 0;
    write_u32(&c, sim_tick);
    write_u32(&c, input_hash);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_bye(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot)
{
    rnet_u8 *c = out;
    if (cap < 16)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_BYE);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    *c++ = 0;
    *c++ = 0;
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_state_begin(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                                  rnet_u8 op, rnet_u8 slot, rnet_u32 xfer_id, rnet_u32 total_size,
                                  rnet_u32 payload_crc)
{
    rnet_u8 *c = out;
    if (cap < 32)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_STATE_BEGIN);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = op;
    *c++ = slot;
    *c++ = 0;
    write_u32(&c, xfer_id);
    write_u32(&c, total_size);
    write_u32(&c, payload_crc);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_state_chunk(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                                  rnet_u32 xfer_id, rnet_u32 offset, const rnet_u8 *data, rnet_u16 size)
{
    rnet_u8 *c = out;
    if (data == NULL || size == 0 || size > RNET_STATE_CHUNK_MAX)
    {
        return -1;
    }
    if (cap < 28u + (size_t)size)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_STATE_CHUNK);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    *c++ = 0;
    *c++ = 0;
    write_u32(&c, xfer_id);
    write_u32(&c, offset);
    write_u16(&c, size);
    write_u16(&c, 0);
    memcpy(c, data, size);
    c += size;
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_state_ack(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                                rnet_u32 xfer_id, rnet_u32 ack_bytes)
{
    rnet_u8 *c = out;
    if (cap < 24)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_STATE_ACK);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    *c++ = 0;
    *c++ = 0;
    write_u32(&c, xfer_id);
    write_u32(&c, ack_bytes);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_state_probe(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                                  rnet_u8 op, rnet_u8 slot, rnet_u32 total_size, rnet_u32 payload_crc)
{
    rnet_u8 *c = out;
    if (cap < 28)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_STATE_PROBE);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = op;
    *c++ = slot;
    *c++ = 0;
    write_u32(&c, total_size);
    write_u32(&c, payload_crc);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_state_probe_reply(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id,
                                        rnet_u8 local_slot, rnet_u8 op, rnet_u8 slot, rnet_u8 match,
                                        rnet_u32 total_size, rnet_u32 payload_crc)
{
    rnet_u8 *c = out;
    if (cap < 28)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_STATE_PROBE_REPLY);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = op;
    *c++ = slot;
    *c++ = match ? 1 : 0;
    write_u32(&c, total_size);
    write_u32(&c, payload_crc);
    return finish_packet(out, c, cap);
}

/* ---- Rollback control packets (reserved range; rollback-mode only) ---- */

int rnet_proto_encode_sio_multi_xfer(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id,
                                     rnet_u8 local_slot, rnet_u8 unit_id, rnet_u32 seq, rnet_u16 send,
                                     rnet_u16 confirm_pad)
{
    rnet_u8 *c = out;
    if (cap < 24)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_SIO_MULTI_XFER);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = unit_id;
    write_u32(&c, seq);
    write_u16(&c, send);
    write_u16(&c, confirm_pad); /* lo=confirm IRQ, hi=vblank mod 256 */
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_rb_sync(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                              rnet_u32 epoch_id, rnet_u32 mismatch_tick, rnet_u32 load_tick, rnet_u32 target_tick,
                              rnet_u8 corrected_slot, rnet_u8 initiator, rnet_u8 flags)
{
    rnet_u8 *c = out;
    if (cap < 36)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_RB_SYNC);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = corrected_slot;
    *c++ = initiator;
    *c++ = flags;
    write_u32(&c, epoch_id);
    write_u32(&c, mismatch_tick);
    write_u32(&c, load_tick);
    write_u32(&c, target_tick);
    return finish_packet(out, c, cap);
}

static void write_rb_frame(rnet_u8 **cursor, const RNetRbWireFrame *f)
{
    write_u16(cursor, f->buttons);
    (*cursor)[0] = (rnet_u8)f->stick_x;
    (*cursor)[1] = (rnet_u8)f->stick_y;
    (*cursor)[2] = f->source;
    (*cursor)[3] = f->is_predicted;
    (*cursor)[4] = f->is_valid;
    *cursor += 5;
}

static void read_rb_frame(const rnet_u8 **cursor, RNetRbWireFrame *f)
{
    f->buttons = read_u16(cursor);
    f->stick_x = (rnet_s8)(*cursor)[0];
    f->stick_y = (rnet_s8)(*cursor)[1];
    f->source = (*cursor)[2];
    f->is_predicted = (*cursor)[3];
    f->is_valid = (*cursor)[4];
    *cursor += 5;
}

int rnet_proto_encode_rb_seal_rows(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id,
                                   rnet_u8 local_slot, rnet_u32 epoch_id, rnet_u32 mismatch_tick,
                                   rnet_u32 target_tick, rnet_u8 slot, rnet_u32 row_begin,
                                   const RNetRbWireFrame *rows, rnet_u16 row_count)
{
    rnet_u8 *c = out;
    rnet_u16 i;

    if ((rows == NULL) && (row_count != 0u))
    {
        return -1;
    }
    if (row_count > RNET_RB_SEAL_ROWS_CHUNK_MAX)
    {
        row_count = RNET_RB_SEAL_ROWS_CHUNK_MAX;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_RB_SEAL_ROWS);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = slot;
    write_u16(&c, row_count);
    write_u32(&c, epoch_id);
    write_u32(&c, mismatch_tick);
    write_u32(&c, target_tick);
    write_u32(&c, row_begin);
    for (i = 0u; i < row_count; ++i)
    {
        write_rb_frame(&c, &rows[i]);
    }
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_rb_baseline(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id,
                                  rnet_u8 local_slot, rnet_u32 epoch_id, rnet_u32 load_tick,
                                  rnet_u32 digest_master, rnet_u32 digest_a, rnet_u32 digest_b, rnet_u32 digest_c)
{
    rnet_u8 *c = out;
    if (cap < 40)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_RB_BASELINE);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    write_u32(&c, epoch_id);
    write_u32(&c, load_tick);
    write_u32(&c, digest_master);
    write_u32(&c, digest_a);
    write_u32(&c, digest_b);
    write_u32(&c, digest_c);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_rb_post(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id, rnet_u8 local_slot,
                              rnet_u32 epoch_id, rnet_u32 target_tick, rnet_u32 digest_master,
                              rnet_u32 input_digest, rnet_u8 match)
{
    rnet_u8 *c = out;
    if (cap < 32)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_RB_POST);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = match ? 1 : 0;
    write_u32(&c, epoch_id);
    write_u32(&c, target_tick);
    write_u32(&c, digest_master);
    write_u32(&c, input_digest);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_rb_frame_commit(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id,
                                      rnet_u8 local_slot, rnet_u32 through_tick, rnet_u32 state_hash)
{
    rnet_u8 *c = out;
    if (cap < 24)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_RB_FRAME_COMMIT);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    write_u32(&c, through_tick);
    write_u32(&c, state_hash);
    return finish_packet(out, c, cap);
}

int rnet_proto_encode_rb_resolved(rnet_u8 *out, size_t cap, rnet_u32 magic, rnet_u32 session_id,
                                  rnet_u8 local_slot, rnet_u32 resolved_through)
{
    rnet_u8 *c = out;
    if (cap < 20)
    {
        return -1;
    }
    write_u32(&c, magic);
    write_u16(&c, RNET_PKT_RB_RESOLVED);
    write_u32(&c, session_id);
    *c++ = local_slot;
    *c++ = 0;
    write_u32(&c, resolved_through);
    return finish_packet(out, c, cap);
}

int rnet_proto_decode(const rnet_u8 *data, size_t len, rnet_u32 expect_magic, RNetDecodedPacket *out)
{
    const rnet_u8 *c;
    const rnet_u8 *end;
    rnet_u32 magic;
    rnet_u32 csum;
    rnet_u32 expect;
    int i;

    if ((data == NULL) || (out == NULL) || (len < 10))
    {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    end = data + len;
    c = data;
    magic = read_u32(&c);
    if (magic != expect_magic)
    {
        return -1;
    }
    out->type = read_u16(&c);
    out->session_id = read_u32(&c);

    /* Verify trailing checksum. */
    if (len < 4)
    {
        return -1;
    }
    {
        const rnet_u8 *tail = data + len - 4;
        expect = (rnet_u32)tail[0] | ((rnet_u32)tail[1] << 8) | ((rnet_u32)tail[2] << 16) | ((rnet_u32)tail[3] << 24);
        csum = rnet_proto_checksum(data, len - 4);
        if (csum != expect)
        {
            return -1;
        }
        end = tail;
    }

    switch (out->type)
    {
    case RNET_PKT_HELLO:
        if ((size_t)(end - c) < 4)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->slot_count = *c++;
        out->delay = *c++;
        (void)*c++;
        break;
    case RNET_PKT_READY:
        if ((size_t)(end - c) < 4)
        {
            return -1;
        }
        out->local_slot = *c++;
        c += 3;
        break;
    case RNET_PKT_START:
        if ((size_t)(end - c) < 4)
        {
            return -1;
        }
        out->start_tick = read_u32(&c);
        break;
    case RNET_PKT_INPUT:
        if ((size_t)(end - c) < 8)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->frame_count = (int)(*c++);
        out->input_epoch = (rnet_u16)(*c++);
        out->input_epoch |= (rnet_u16)((rnet_u16)(*c++) << 8);
        out->ack_tick = read_u32(&c);
        if (out->frame_count < 1 || out->frame_count > RNET_MAX_BUNDLE)
        {
            return -1;
        }
        for (i = 0; i < out->frame_count; ++i)
        {
            rnet_u16 sz;
            if ((size_t)(end - c) < 6)
            {
                return -1;
            }
            out->frames[i].tick = read_u32(&c);
            sz = read_u16(&c);
            if (sz > RNET_INPUT_MAX || (size_t)(end - c) < sz)
            {
                return -1;
            }
            out->frames[i].size = sz;
            if (sz > 0)
            {
                memcpy(out->frames[i].bytes, c, sz);
                c += sz;
            }
        }
        break;
    case RNET_PKT_DELAY_SYNC:
        if ((size_t)(end - c) < 8)
        {
            return -1;
        }
        out->new_delay = *c++;
        c += 3;
        out->effective_tick = read_u32(&c);
        break;
    case RNET_PKT_INPUT_CONFIRM:
        if ((size_t)(end - c) < 12)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->input_epoch = (rnet_u16)(*c++);
        out->input_epoch |= (rnet_u16)((rnet_u16)(*c++) << 8);
        c += 1;
        out->confirm_sim_tick = read_u32(&c);
        out->confirm_hash = read_u32(&c);
        break;
    case RNET_PKT_BYE:
        if ((size_t)(end - c) < 4)
        {
            return -1;
        }
        out->local_slot = *c++;
        c += 3;
        break;
    case RNET_PKT_STATE_BEGIN:
        if ((size_t)(end - c) < 16)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->state_op = *c++;
        out->state_slot = *c++;
        c++;
        out->state_xfer_id = read_u32(&c);
        out->state_total_size = read_u32(&c);
        out->state_payload_crc = read_u32(&c);
        break;
    case RNET_PKT_STATE_CHUNK:
        if ((size_t)(end - c) < 16)
        {
            return -1;
        }
        out->local_slot = *c++;
        c += 3;
        out->state_xfer_id = read_u32(&c);
        out->state_offset = read_u32(&c);
        out->state_chunk_size = read_u16(&c);
        (void)read_u16(&c);
        if (out->state_chunk_size == 0 || out->state_chunk_size > RNET_STATE_CHUNK_MAX ||
            (size_t)(end - c) < out->state_chunk_size)
        {
            return -1;
        }
        memcpy(out->state_chunk, c, out->state_chunk_size);
        c += out->state_chunk_size;
        break;
    case RNET_PKT_STATE_ACK:
        if ((size_t)(end - c) < 12)
        {
            return -1;
        }
        out->local_slot = *c++;
        c += 3;
        out->state_xfer_id = read_u32(&c);
        out->state_ack_bytes = read_u32(&c);
        break;
    case RNET_PKT_STATE_PROBE:
        if ((size_t)(end - c) < 12)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->state_op = *c++;
        out->state_slot = *c++;
        c++;
        out->state_total_size = read_u32(&c);
        out->state_payload_crc = read_u32(&c);
        break;
    case RNET_PKT_STATE_PROBE_REPLY:
        /* match + echoed size/crc (binds reply to one probe generation). */
        if ((size_t)(end - c) < 12)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->state_op = *c++;
        out->state_slot = *c++;
        out->state_probe_match = *c++;
        out->state_total_size = read_u32(&c);
        out->state_payload_crc = read_u32(&c);
        break;
    case RNET_PKT_SIO_MULTI_XFER:
        if ((size_t)(end - c) < 10)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->sio_unit_id = *c++;
        out->sio_xfer_seq = read_u32(&c);
        out->sio_send = read_u16(&c);
        {
            rnet_u16 pad = read_u16(&c);
            out->sio_confirm = (rnet_u8)(pad & 0xFFu);
            out->sio_vblank = (rnet_u8)((pad >> 8) & 0xFFu);
        }
        break;
    case RNET_PKT_RB_SYNC:
        if ((size_t)(end - c) < 20)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->rb_corrected_slot = *c++;
        out->rb_initiator = *c++;
        out->rb_flags = *c++;
        out->rb_epoch_id = read_u32(&c);
        out->rb_mismatch_tick = read_u32(&c);
        out->rb_load_tick = read_u32(&c);
        out->rb_target_tick = read_u32(&c);
        break;
    case RNET_PKT_RB_SEAL_ROWS:
    {
        rnet_u16 i;
        if ((size_t)(end - c) < 20)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->rb_slot = *c++;
        out->rb_row_count = read_u16(&c);
        out->rb_epoch_id = read_u32(&c);
        out->rb_mismatch_tick = read_u32(&c);
        out->rb_target_tick = read_u32(&c);
        out->rb_row_begin = read_u32(&c);
        if (out->rb_row_count > RNET_RB_SEAL_ROWS_CHUNK_MAX)
        {
            return -1;
        }
        if ((size_t)(end - c) < ((size_t)out->rb_row_count * 5u))
        {
            return -1;
        }
        for (i = 0u; i < out->rb_row_count; ++i)
        {
            read_rb_frame(&c, &out->rb_rows[i]);
        }
        break;
    }
    case RNET_PKT_RB_BASELINE:
        if ((size_t)(end - c) < 26)
        {
            return -1;
        }
        out->local_slot = *c++;
        (void)*c++;
        out->rb_epoch_id = read_u32(&c);
        out->rb_load_tick = read_u32(&c);
        out->rb_digest_master = read_u32(&c);
        out->rb_digest_a = read_u32(&c);
        out->rb_digest_b = read_u32(&c);
        out->rb_digest_c = read_u32(&c);
        break;
    case RNET_PKT_RB_POST:
        if ((size_t)(end - c) < 18)
        {
            return -1;
        }
        out->local_slot = *c++;
        out->rb_match = *c++;
        out->rb_epoch_id = read_u32(&c);
        out->rb_target_tick = read_u32(&c);
        out->rb_digest_master = read_u32(&c);
        out->rb_input_digest = read_u32(&c);
        break;
    case RNET_PKT_RB_FRAME_COMMIT:
        if ((size_t)(end - c) < 10)
        {
            return -1;
        }
        out->local_slot = *c++;
        (void)*c++;
        out->rb_through_tick = read_u32(&c);
        out->rb_state_hash = read_u32(&c);
        break;
    case RNET_PKT_RB_RESOLVED:
        if ((size_t)(end - c) < 6)
        {
            return -1;
        }
        out->local_slot = *c++;
        (void)*c++;
        out->rb_resolved_through = read_u32(&c);
        break;
    default:
        return -1;
    }
    return 0;
}
