#include "retcomm_rbengine/input_hist.h"

#include <string.h>

void rbe_ih_reset(RbeInputHist *h, int slot_count)
{
    if (!h)
        return;
    memset(h, 0, sizeof(*h));
    if (slot_count < 1)
        slot_count = 1;
    if (slot_count > RBE_INPUT_HIST_MAX_SLOTS)
        slot_count = RBE_INPUT_HIST_MAX_SLOTS;
    h->slot_count = slot_count;
}

void rbe_ih_frame_to_contract(const RNetRbFrame *frame, RNetInputContractFrame *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!frame)
        return;
    out->tick = frame->tick;
    out->buttons = frame->buttons;
    out->stick_x = frame->stick_x;
    out->stick_y = frame->stick_y;
    out->is_predicted = frame->is_predicted ? 1u : 0u;
}

int rbe_ih_put(RbeInputHist *h, int slot, const RNetRbFrame *frame)
{
    RNetRbFrame *dst;
    if (!h || !frame || !frame->is_valid)
        return 0;
    if (slot < 0 || slot >= h->slot_count)
        return 0;
    dst = &h->rows[slot][frame->tick % RBE_INPUT_HIST_DEPTH];
    *dst = *frame;
    dst->is_valid = 1u;
    return 1;
}

int rbe_ih_get(const RbeInputHist *h, int slot, uint32_t tick, RNetRbFrame *out)
{
    const RNetRbFrame *src;
    if (!h || !out)
        return 0;
    if (slot < 0 || slot >= h->slot_count)
        return 0;
    src = &h->rows[slot][tick % RBE_INPUT_HIST_DEPTH];
    if (!src->is_valid || src->tick != tick)
        return 0;
    *out = *src;
    return 1;
}

int rbe_ih_invent_hold_last(RbeInputHist *h, int slot, uint32_t tick, RNetRbFrame *out)
{
    RNetRbFrame invented;
    RNetRbFrame prev;
    uint32_t look;

    if (!h || slot < 0 || slot >= h->slot_count)
        return 0;

    memset(&invented, 0, sizeof(invented));
    invented.tick = tick;
    invented.buttons = 0xFFFFu;
    invented.analog = 0u;
    invented.is_predicted = 1u;
    invented.is_valid = 1u;

    for (look = 1; look < RBE_INPUT_HIST_DEPTH; look++) {
        if (tick < look)
            break;
        if (rbe_ih_get(h, slot, tick - look, &prev)) {
            invented.buttons = prev.buttons;
            invented.stick_x = prev.stick_x;
            invented.stick_y = prev.stick_y;
            invented.analog = prev.analog ? 1u : 0u;
            break;
        }
    }

    if (!rbe_ih_put(h, slot, &invented))
        return 0;
    h->invent_count++;
    if (out)
        *out = invented;
    return 1;
}

int rbe_ih_invent_idle(RbeInputHist *h, int slot, uint32_t tick, RNetRbFrame *out)
{
    RNetRbFrame invented;

    if (!h || slot < 0 || slot >= h->slot_count)
        return 0;

    memset(&invented, 0, sizeof(invented));
    invented.tick = tick;
    invented.buttons = 0xFFFFu;
    invented.analog = 0u;
    invented.is_predicted = 1u;
    invented.is_valid = 1u;

    if (!rbe_ih_put(h, slot, &invented))
        return 0;
    h->invent_count++;
    if (out)
        *out = invented;
    return 1;
}

int rbe_ih_promote(RbeInputHist *h, int slot, const RNetRbFrame *wire)
{
    RNetRbFrame auth;
    if (!h || !wire || !wire->is_valid)
        return 0;
    auth = *wire;
    auth.is_predicted = 0u;
    auth.is_valid = 1u;
    if (!rbe_ih_put(h, slot, &auth))
        return 0;
    h->promote_count++;
    return 1;
}
