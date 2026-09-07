/*
 * Portable GGPO-style input replace contract — pure decision core.
 *
 * Exported verbatim from BattleShip netinput_contract.c (soak-hardened policy);
 * every branch is load-bearing against a documented soak. Keep this TU free of
 * engine includes so any recomp host can drop it in.
 */

#include "recomp_net/input_contract.h"

#include <stddef.h>

#define RNET_INPUT_CONTRACT_TRUE ((uint8_t)1)
#define RNET_INPUT_CONTRACT_FALSE ((uint8_t)0)

void rnet_input_contract_params_init_defaults(RNetInputContractParams *params)
{
    if (params == NULL)
    {
        return;
    }
    params->confirmed_deadband = 12U;
    params->predict_deadband = 14U;
    params->micro_deadband = 3U;
    params->continuity_deadband = 12U;
    params->analog_min_mag = 12U;
    params->same_intent_min_active = 8U;
    params->same_intent_tolerance = 14U;
    params->onset_facing_thresh = 4U;
    params->onset_large_delta = 40U;
    params->dash_gate_min = 56;
    params->digital_axis_mag = 85;
}

static int32_t rnet_input_contract_abs_s8_diff(int8_t a, int8_t b)
{
    int32_t d;

    d = (int32_t)a - (int32_t)b;
    if (d < 0)
    {
        return -d;
    }
    return d;
}

static int32_t rnet_input_contract_stick_sign(int8_t axis)
{
    if (axis > 0)
    {
        return 1;
    }
    if (axis < 0)
    {
        return -1;
    }
    return 0;
}

/* Digital keyboard encoding: full stick deflection on one axis. */
static uint8_t rnet_input_contract_stick_axis_is_digital(int8_t axis,
                                                         const RNetInputContractParams *params)
{
    if (params->digital_axis_mag == 0)
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    return ((axis == params->digital_axis_mag) || (axis == (int8_t)-params->digital_axis_mag))
               ? RNET_INPUT_CONTRACT_TRUE
               : RNET_INPUT_CONTRACT_FALSE;
}

static uint8_t rnet_input_contract_sticks_near_neutral(const RNetInputContractFrame *frame,
                                                       uint32_t deadband)
{
    if (frame == NULL)
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    return ((rnet_input_contract_abs_s8_diff(frame->stick_x, 0) <= (int32_t)deadband) &&
            (rnet_input_contract_abs_s8_diff(frame->stick_y, 0) <= (int32_t)deadband))
               ? RNET_INPUT_CONTRACT_TRUE
               : RNET_INPUT_CONTRACT_FALSE;
}

uint8_t rnet_input_contract_stick_looks_analog(int8_t stick_x, int8_t stick_y,
                                               const RNetInputContractParams *params)
{
    if ((rnet_input_contract_abs_s8_diff(stick_x, 0) <= (int32_t)params->analog_min_mag) &&
        (rnet_input_contract_abs_s8_diff(stick_y, 0) <= (int32_t)params->analog_min_mag))
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    if ((rnet_input_contract_stick_axis_is_digital(stick_x, params) != RNET_INPUT_CONTRACT_FALSE) ||
        (rnet_input_contract_stick_axis_is_digital(stick_y, params) != RNET_INPUT_CONTRACT_FALSE))
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    return RNET_INPUT_CONTRACT_TRUE;
}

uint8_t rnet_input_contract_stick_same_analog_intent(int8_t ax, int8_t ay, int8_t bx, int8_t by,
                                                     const RNetInputContractParams *params)
{
    int32_t min_active;

    if ((rnet_input_contract_stick_looks_analog(ax, ay, params) == RNET_INPUT_CONTRACT_FALSE) ||
        (rnet_input_contract_stick_looks_analog(bx, by, params) == RNET_INPUT_CONTRACT_FALSE))
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    min_active = (int32_t)params->same_intent_min_active;
    /* Use >= min_active so |s|==min_active participates in sign checks. */
    if ((rnet_input_contract_abs_s8_diff(ax, 0) >= min_active) ||
        (rnet_input_contract_abs_s8_diff(bx, 0) >= min_active))
    {
        if ((rnet_input_contract_abs_s8_diff(ax, 0) >= min_active) &&
            (rnet_input_contract_abs_s8_diff(bx, 0) >= min_active) &&
            (rnet_input_contract_stick_sign(ax) != rnet_input_contract_stick_sign(bx)))
        {
            return RNET_INPUT_CONTRACT_FALSE;
        }
    }
    if ((rnet_input_contract_abs_s8_diff(ay, 0) >= min_active) ||
        (rnet_input_contract_abs_s8_diff(by, 0) >= min_active))
    {
        if ((rnet_input_contract_abs_s8_diff(ay, 0) >= min_active) &&
            (rnet_input_contract_abs_s8_diff(by, 0) >= min_active) &&
            (rnet_input_contract_stick_sign(ay) != rnet_input_contract_stick_sign(by)))
        {
            return RNET_INPUT_CONTRACT_FALSE;
        }
    }
    return RNET_INPUT_CONTRACT_TRUE;
}

static uint8_t rnet_input_contract_stick_dash_gate_active_x(int8_t stick_x,
                                                            const RNetInputContractParams *params)
{
    return (rnet_input_contract_abs_s8_diff(stick_x, 0) >= params->dash_gate_min)
               ? RNET_INPUT_CONTRACT_TRUE
               : RNET_INPUT_CONTRACT_FALSE;
}

/* |sx| crosses the smash threshold and/or X sign flips while either side is
 * smash-class. Disabled when dash_gate_min <= 0. */
uint8_t rnet_input_contract_stick_dash_gate_disagree_x(int8_t hold_x, int8_t wire_x,
                                                       const RNetInputContractParams *params)
{
    uint8_t hold_dash;
    uint8_t wire_dash;
    int32_t hold_sign;
    int32_t wire_sign;

    if (params->dash_gate_min <= 0)
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    hold_dash = rnet_input_contract_stick_dash_gate_active_x(hold_x, params);
    wire_dash = rnet_input_contract_stick_dash_gate_active_x(wire_x, params);
    if (hold_dash != wire_dash)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    hold_sign = rnet_input_contract_stick_sign(hold_x);
    wire_sign = rnet_input_contract_stick_sign(wire_x);
    if ((hold_dash != RNET_INPUT_CONTRACT_FALSE) && (hold_sign != 0) && (wire_sign != 0) &&
        (hold_sign != wire_sign))
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    return RNET_INPUT_CONTRACT_FALSE;
}

/* Release (published analog -> wire nearer/at neutral) always rewinds on the
 * completed-sim path and never defers. */
uint8_t rnet_input_contract_stick_replace_is_release(const RNetInputContractFrame *old_frame,
                                                     const RNetInputContractFrame *wire,
                                                     const RNetInputContractParams *params)
{
    uint32_t confirmed_db;
    int32_t old_mag;
    int32_t wire_mag;

    if ((old_frame == NULL) || (wire == NULL))
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    confirmed_db = params->confirmed_deadband;
    if (rnet_input_contract_sticks_near_neutral(old_frame, confirmed_db) !=
        RNET_INPUT_CONTRACT_FALSE)
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    if (rnet_input_contract_sticks_near_neutral(wire, confirmed_db) != RNET_INPUT_CONTRACT_FALSE)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    old_mag = rnet_input_contract_abs_s8_diff(old_frame->stick_x, 0);
    if (rnet_input_contract_abs_s8_diff(old_frame->stick_y, 0) > old_mag)
    {
        old_mag = rnet_input_contract_abs_s8_diff(old_frame->stick_y, 0);
    }
    wire_mag = rnet_input_contract_abs_s8_diff(wire->stick_x, 0);
    if (rnet_input_contract_abs_s8_diff(wire->stick_y, 0) > wire_mag)
    {
        wire_mag = rnet_input_contract_abs_s8_diff(wire->stick_y, 0);
    }
    /* Clearly shedding magnitude (not a same-intent ramp up). */
    return ((wire_mag + (int32_t)confirmed_db) < old_mag) ? RNET_INPUT_CONTRACT_TRUE
                                                          : RNET_INPUT_CONTRACT_FALSE;
}

/* Predicted +/-digital on one axis vs remote neutral/partial on that axis —
 * heuristic promotion artifact, not a committed digital jump. */
static uint8_t
rnet_input_contract_false_digital_heuristic_mismatch(const RNetInputContractFrame *published,
                                                     const RNetInputContractFrame *remote,
                                                     const RNetInputContractParams *params)
{
    int32_t weak_thresh;

    if ((published == NULL) || (remote == NULL))
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    weak_thresh = 25;
    if ((rnet_input_contract_stick_axis_is_digital(published->stick_y, params) !=
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_stick_axis_is_digital(remote->stick_y, params) ==
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_abs_s8_diff(published->stick_x, 0) <= 14) &&
        (rnet_input_contract_abs_s8_diff(remote->stick_x, 0) <= 14) &&
        (rnet_input_contract_abs_s8_diff(remote->stick_y, 0) <= weak_thresh))
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if ((rnet_input_contract_stick_axis_is_digital(published->stick_x, params) !=
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_stick_axis_is_digital(remote->stick_x, params) ==
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_abs_s8_diff(published->stick_y, 0) <= 14) &&
        (rnet_input_contract_abs_s8_diff(remote->stick_y, 0) <= 14) &&
        (rnet_input_contract_abs_s8_diff(remote->stick_x, 0) <= weak_thresh))
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    return RNET_INPUT_CONTRACT_FALSE;
}

uint8_t rnet_input_contract_correction_is_significant(const RNetInputContractFrame *old_frame,
                                                      const RNetInputContractFrame *new_frame,
                                                      uint8_t correction_is_predicted,
                                                      const RNetInputContractParams *params)
{
    uint32_t deadband;
    uint32_t facing_thresh;
    uint32_t large_delta;

    if ((old_frame == NULL) || (new_frame == NULL))
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if (old_frame->buttons != new_frame->buttons)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if ((correction_is_predicted != RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_false_digital_heuristic_mismatch(old_frame, new_frame, params) !=
         RNET_INPUT_CONTRACT_FALSE))
    {
        return RNET_INPUT_CONTRACT_FALSE;
    }
    if (correction_is_predicted != RNET_INPUT_CONTRACT_FALSE)
    {
        deadband = params->predict_deadband;
    }
    else
    {
        deadband = params->confirmed_deadband;
    }
    facing_thresh = params->onset_facing_thresh;
    large_delta = params->onset_large_delta;
    if (rnet_input_contract_sticks_near_neutral(old_frame, deadband) != RNET_INPUT_CONTRACT_FALSE)
    {
        if (rnet_input_contract_sticks_near_neutral(new_frame, deadband) ==
            RNET_INPUT_CONTRACT_FALSE)
        {
            return RNET_INPUT_CONTRACT_TRUE;
        }
        if ((rnet_input_contract_abs_s8_diff(new_frame->stick_x, 0) > 25) ||
            (rnet_input_contract_abs_s8_diff(new_frame->stick_y, 0) > 25))
        {
            return RNET_INPUT_CONTRACT_TRUE;
        }
    }
    if ((rnet_input_contract_abs_s8_diff(old_frame->stick_x, 0) > (int32_t)facing_thresh) &&
        (rnet_input_contract_abs_s8_diff(new_frame->stick_x, 0) > (int32_t)facing_thresh) &&
        (rnet_input_contract_stick_sign(old_frame->stick_x) !=
         rnet_input_contract_stick_sign(new_frame->stick_x)))
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if (rnet_input_contract_abs_s8_diff(old_frame->stick_x, new_frame->stick_x) >
        (int32_t)large_delta)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if (rnet_input_contract_abs_s8_diff(old_frame->stick_y, new_frame->stick_y) >
        (int32_t)large_delta)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if (deadband == 0U)
    {
        return ((old_frame->stick_x != new_frame->stick_x) ||
                (old_frame->stick_y != new_frame->stick_y))
                   ? RNET_INPUT_CONTRACT_TRUE
                   : RNET_INPUT_CONTRACT_FALSE;
    }
    if (rnet_input_contract_abs_s8_diff(old_frame->stick_x, new_frame->stick_x) >
        (int32_t)deadband)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if (rnet_input_contract_abs_s8_diff(old_frame->stick_y, new_frame->stick_y) >
        (int32_t)deadband)
    {
        return RNET_INPUT_CONTRACT_TRUE;
    }
    if ((correction_is_predicted != RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_sticks_near_neutral(old_frame, deadband) ==
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_sticks_near_neutral(new_frame, deadband) ==
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_stick_same_analog_intent(old_frame->stick_x, old_frame->stick_y,
                                                      new_frame->stick_x, new_frame->stick_y,
                                                      params) != RNET_INPUT_CONTRACT_FALSE))
    {
        uint32_t same_intent_tol;

        same_intent_tol = params->same_intent_tolerance;
        if ((rnet_input_contract_abs_s8_diff(old_frame->stick_x, new_frame->stick_x) <=
             (int32_t)same_intent_tol) &&
            (rnet_input_contract_abs_s8_diff(old_frame->stick_y, new_frame->stick_y) <=
             (int32_t)same_intent_tol))
        {
            return RNET_INPUT_CONTRACT_FALSE;
        }
    }
    return RNET_INPUT_CONTRACT_FALSE;
}

RNetInputContractCorrectionClass
rnet_input_contract_classify_correction(const RNetInputContractFrame *old_frame,
                                        const RNetInputContractFrame *wire,
                                        const RNetInputContractParams *params)
{
    uint32_t micro_db;
    uint32_t continuity_db;
    int32_t dx;
    int32_t dy;

    if (old_frame->buttons != wire->buttons)
    {
        return nRNetInputContractClassButton;
    }
    if (rnet_input_contract_stick_replace_is_release(old_frame, wire, params) !=
        RNET_INPUT_CONTRACT_FALSE)
    {
        return nRNetInputContractClassRelease;
    }
    if ((rnet_input_contract_sticks_near_neutral(old_frame, params->predict_deadband) !=
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_sticks_near_neutral(wire, params->predict_deadband) ==
         RNET_INPUT_CONTRACT_FALSE))
    {
        return nRNetInputContractClassOnsetFromZero;
    }
    micro_db = params->micro_deadband;
    continuity_db = params->continuity_deadband;
    dx = rnet_input_contract_abs_s8_diff(old_frame->stick_x, wire->stick_x);
    dy = rnet_input_contract_abs_s8_diff(old_frame->stick_y, wire->stick_y);
    if ((rnet_input_contract_stick_looks_analog(old_frame->stick_x, old_frame->stick_y, params) !=
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_stick_looks_analog(wire->stick_x, wire->stick_y, params) !=
         RNET_INPUT_CONTRACT_FALSE) &&
        (rnet_input_contract_stick_same_analog_intent(old_frame->stick_x, old_frame->stick_y,
                                                      wire->stick_x, wire->stick_y,
                                                      params) != RNET_INPUT_CONTRACT_FALSE))
    {
        if ((micro_db > 0U) && (dx <= (int32_t)micro_db) && (dy <= (int32_t)micro_db))
        {
            return nRNetInputContractClassMicroStick;
        }
        if ((continuity_db > micro_db) && (dx <= (int32_t)continuity_db) &&
            (dy <= (int32_t)continuity_db))
        {
            return nRNetInputContractClassSameIntentContinuity;
        }
    }
    return nRNetInputContractClassRealStick;
}

static uint8_t rnet_input_contract_frame_gameplay_equals(const RNetInputContractFrame *a,
                                                         const RNetInputContractFrame *b)
{
    return ((a->tick == b->tick) && (a->buttons == b->buttons) && (a->stick_x == b->stick_x) &&
            (a->stick_y == b->stick_y))
               ? RNET_INPUT_CONTRACT_TRUE
               : RNET_INPUT_CONTRACT_FALSE;
}

RNetInputContractDecision
rnet_input_contract_stick_replace_decide(const RNetInputContractFrame *published,
                                         const RNetInputContractFrame *wire,
                                         uint8_t completed_sim,
                                         const RNetInputContractParams *params,
                                         const RNetInputContractHostGates *gates)
{
    void *gate_ctx;

    gate_ctx = (gates != NULL) ? gates->ctx : NULL;
    if (rnet_input_contract_frame_gameplay_equals(published, wire) != RNET_INPUT_CONTRACT_FALSE)
    {
        /* Same sticks can still hide a host branch miss; predicted rows rewind
         * when the host holds a deferred ticket for this tick. */
        if ((published->is_predicted != RNET_INPUT_CONTRACT_FALSE) && (gates != NULL) &&
            (gates->equal_predicted_force_rewind != NULL) &&
            (gates->equal_predicted_force_rewind(gate_ctx) != RNET_INPUT_CONTRACT_FALSE))
        {
            return nRNetInputContractRewindEqualDeferred;
        }
        return nRNetInputContractPromoteEqual;
    }
    /* Buttons-equal stick delta the host knows cannot affect the hashed sim. */
    if ((published->buttons == wire->buttons) && (gates != NULL) &&
        (gates->absorb_stick_replace != NULL) &&
        (gates->absorb_stick_replace(gate_ctx) != RNET_INPUT_CONTRACT_FALSE))
    {
        return nRNetInputContractPromoteAbsorb;
    }
    if (completed_sim != RNET_INPUT_CONTRACT_FALSE)
    {
        uint32_t micro_db;
        uint32_t continuity_db;
        int32_t dx;
        int32_t dy;

        if (published->buttons != wire->buttons)
        {
            return nRNetInputContractRewind;
        }
        if (rnet_input_contract_stick_replace_is_release(published, wire, params) !=
            RNET_INPUT_CONTRACT_FALSE)
        {
            return nRNetInputContractRewind;
        }
        micro_db = params->micro_deadband;
        continuity_db = params->continuity_deadband;
        dx = rnet_input_contract_abs_s8_diff(published->stick_x, wire->stick_x);
        dy = rnet_input_contract_abs_s8_diff(published->stick_y, wire->stick_y);
        /* Same-intent Promote-only window; never promote when the dash-gate X
         * proxy disagrees (smash threshold cross / sign flip). */
        if ((rnet_input_contract_stick_looks_analog(published->stick_x, published->stick_y,
                                                    params) != RNET_INPUT_CONTRACT_FALSE) &&
            (rnet_input_contract_stick_looks_analog(wire->stick_x, wire->stick_y, params) !=
             RNET_INPUT_CONTRACT_FALSE) &&
            (rnet_input_contract_stick_same_analog_intent(published->stick_x, published->stick_y,
                                                          wire->stick_x, wire->stick_y,
                                                          params) != RNET_INPUT_CONTRACT_FALSE) &&
            (rnet_input_contract_stick_dash_gate_disagree_x(published->stick_x, wire->stick_x,
                                                            params) == RNET_INPUT_CONTRACT_FALSE))
        {
            /* Host move-protect (may exceed deadbands). */
            if ((gates != NULL) && (gates->protect_promote != NULL) &&
                (gates->protect_promote(gate_ctx, dx, dy, micro_db, published->is_predicted) !=
                 RNET_INPUT_CONTRACT_FALSE))
            {
                return nRNetInputContractPromoteProtect;
            }
            /* Fragile aim windows: host blocks every deadband promote below. */
            if ((gates == NULL) || (gates->block_deadband_promote == NULL) ||
                (gates->block_deadband_promote(gate_ctx) == RNET_INPUT_CONTRACT_FALSE))
            {
                if (published->is_predicted == RNET_INPUT_CONTRACT_FALSE)
                {
                    if ((micro_db > 0U) && (dx <= (int32_t)micro_db) &&
                        (dy <= (int32_t)micro_db))
                    {
                        return nRNetInputContractPromoteMicro;
                    }
                    if ((continuity_db > micro_db) && (dx <= (int32_t)continuity_db) &&
                        (dy <= (int32_t)continuity_db))
                    {
                        return nRNetInputContractPromoteContinuity;
                    }
                }
                else if ((dx <= (int32_t)continuity_db) && (dy <= (int32_t)continuity_db) &&
                         (gates != NULL) && (gates->hash_confirm_promote != NULL) &&
                         (gates->hash_confirm_promote(gate_ctx) != RNET_INPUT_CONTRACT_FALSE))
                {
                    /* Predicted rows never get a bare deadband promote — only a
                     * peer state watermark past this tick. */
                    return nRNetInputContractPromoteHashConfirm;
                }
            }
        }
        /* Any remaining stick/button gameplay delta on completed sim rewinds. */
        return nRNetInputContractRewind;
    }
    /* Runway: analog -> neutral / shedding magnitude always rewinds before any defer. */
    if (rnet_input_contract_stick_replace_is_release(published, wire, params) !=
        RNET_INPUT_CONTRACT_FALSE)
    {
        return nRNetInputContractRewind;
    }
    if ((gates != NULL) && (gates->defer_predicted_correction != NULL) &&
        (gates->defer_predicted_correction(gate_ctx) != RNET_INPUT_CONTRACT_FALSE))
    {
        return nRNetInputContractPromoteDefer;
    }
    if (rnet_input_contract_correction_is_significant(published, wire, RNET_INPUT_CONTRACT_FALSE,
                                                      params) != RNET_INPUT_CONTRACT_FALSE)
    {
        return nRNetInputContractRewind;
    }
    return nRNetInputContractPromoteInsignificant;
}
