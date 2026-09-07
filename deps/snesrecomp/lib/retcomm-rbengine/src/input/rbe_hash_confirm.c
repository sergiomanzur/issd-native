#include "retcomm_rbengine/hash_confirm.h"

#include <string.h>

static uint32_t slot_of(uint32_t tick)
{
    return tick % RBE_HC_RING;
}

void rbe_hc_reset(RbeHashConfirm *hc)
{
    if (!hc)
        return;
    memset(hc, 0, sizeof(*hc));
}

void rbe_hc_prime_after(RbeHashConfirm *hc, uint32_t last_ok)
{
    if (!hc)
        return;
    memset(hc, 0, sizeof(*hc));
    hc->resolved_through = last_ok;
    hc->resolved_valid = 1u;
}

static void try_advance(RbeHashConfirm *hc);

void rbe_hc_note_local(RbeHashConfirm *hc, uint32_t tick, uint32_t digest)
{
    uint32_t i;
    if (!hc)
        return;
    i = slot_of(tick);
    hc->local_tick[i] = tick;
    hc->local_digest[i] = digest;
    hc->local_valid[i] = 1u;
    try_advance(hc);
}

static int local_at(const RbeHashConfirm *hc, uint32_t tick, uint32_t *dig)
{
    uint32_t i = slot_of(tick);
    if (!hc->local_valid[i] || hc->local_tick[i] != tick)
        return 0;
    if (dig)
        *dig = hc->local_digest[i];
    return 1;
}

static int peer_at(const RbeHashConfirm *hc, uint32_t tick, uint32_t *dig)
{
    uint32_t i = slot_of(tick);
    if (!hc->peer_valid[i] || hc->peer_tick[i] != tick)
        return 0;
    if (dig)
        *dig = hc->peer_digest[i];
    return 1;
}

static void try_advance(RbeHashConfirm *hc)
{
    for (;;) {
        uint32_t next;
        uint32_t ld = 0, pd = 0;
        if (!hc->resolved_valid)
            next = 0u;
        else {
            if (hc->resolved_through == 0xffffffffu)
                return;
            next = hc->resolved_through + 1u;
        }
        if (!local_at(hc, next, &ld) || !peer_at(hc, next, &pd))
            return;
        if (ld != pd)
            return;
        hc->resolved_through = next;
        hc->resolved_valid = 1u;
    }
}

void rbe_hc_note_peer(RbeHashConfirm *hc, uint32_t tick, uint32_t digest)
{
    uint32_t i;
    if (!hc)
        return;
    i = slot_of(tick);
    hc->peer_tick[i] = tick;
    hc->peer_digest[i] = digest;
    hc->peer_valid[i] = 1u;
    try_advance(hc);
}

uint32_t rbe_hc_resolved_through(const RbeHashConfirm *hc)
{
    if (!hc || !hc->resolved_valid)
        return 0u;
    return hc->resolved_through;
}

uint8_t rbe_hc_confirm_through(const RbeHashConfirm *hc, uint32_t tick)
{
    if (!hc || !hc->resolved_valid)
        return 0u;
    return (tick <= hc->resolved_through) ? 1u : 0u;
}

uint8_t rbe_hc_local_digest(const RbeHashConfirm *hc, uint32_t tick,
                            uint32_t *digest_out)
{
    uint32_t d = 0;
    if (!hc || !local_at(hc, tick, &d))
        return 0u;
    if (digest_out)
        *digest_out = d;
    return 1u;
}

uint8_t rbe_hc_peer_digest(const RbeHashConfirm *hc, uint32_t tick,
                           uint32_t *digest_out)
{
    uint32_t d = 0;
    if (!hc || !peer_at(hc, tick, &d))
        return 0u;
    if (digest_out)
        *digest_out = d;
    return 1u;
}

uint8_t rbe_hc_peek_mismatch(const RbeHashConfirm *hc, uint32_t *tick_out,
                             uint32_t *local_out, uint32_t *peer_out)
{
    uint32_t next;
    uint32_t ld = 0, pd = 0;
    if (!hc)
        return 0u;
    if (!hc->resolved_valid)
        next = 0u;
    else {
        if (hc->resolved_through == 0xffffffffu)
            return 0u;
        next = hc->resolved_through + 1u;
    }
    if (!local_at(hc, next, &ld) || !peer_at(hc, next, &pd))
        return 0u;
    if (ld == pd)
        return 0u;
    if (tick_out)
        *tick_out = next;
    if (local_out)
        *local_out = ld;
    if (peer_out)
        *peer_out = pd;
    return 1u;
}

uint8_t rbe_hc_heal_stale_gap(RbeHashConfirm *hc)
{
    uint32_t next;
    uint32_t best = 0u;
    uint8_t have_best = 0u;
    uint32_t i;
    uint32_t ld = 0, pd = 0;
    if (!hc || !hc->resolved_valid)
        return 0u;
    if (hc->resolved_through == 0xffffffffu)
        return 0u;
    next = hc->resolved_through + 1u;
    if (local_at(hc, next, &ld) && peer_at(hc, next, &pd))
        return 0u;
    if (local_at(hc, next, NULL) || peer_at(hc, next, NULL))
        return 0u;
    for (i = 0; i < RBE_HC_RING; i++) {
        uint32_t t;
        if (!hc->local_valid[i] || !hc->peer_valid[i])
            continue;
        if (hc->local_tick[i] != hc->peer_tick[i])
            continue;
        t = hc->local_tick[i];
        if (t <= hc->resolved_through)
            continue;
        if (hc->local_digest[i] != hc->peer_digest[i])
            return 0u;
        if (!have_best || t > best) {
            best = t;
            have_best = 1u;
        }
    }
    if (!have_best || best <= hc->resolved_through)
        return 0u;
    hc->resolved_through = best;
    return 1u;
}
