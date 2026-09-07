#ifndef RECOMP_NET_INPUT_CONTRACT_H
#define RECOMP_NET_INPUT_CONTRACT_H

/*
 * Portable GGPO-style input replace contract — pure decision core.
 *
 * Standalone TU: stdint.h only. No engine includes, no rings, no env reads, no
 * logging, no game/status knowledge. Game- and engine-specific behavior enters
 * only through RNetInputContractParams (numeric thresholds) and
 * RNetInputContractHostGates (optional host callbacks, queried lazily in
 * decision order).
 *
 * Semantics are exported verbatim from BattleShip's soak-hardened
 * netinput_contract (docs/netplay_input_contract_portable.md in that repo).
 * Every branch is load-bearing against a documented soak — do not relax
 * without a new soak.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RNetInputContractFrame
{
    uint32_t tick;
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
    uint8_t is_predicted;
} RNetInputContractFrame;

typedef struct RNetInputContractParams
{
    /* Runway significance deadband for confirmed corrections (default 12). */
    uint32_t confirmed_deadband;
    /* Runway significance deadband for predicted corrections (default 14). */
    uint32_t predict_deadband;
    /* Completed-sim confirmed same-intent noise floor, Promote-only (default 3; 0 disables). */
    uint32_t micro_deadband;
    /* Completed-sim confirmed same-intent mag drift, Promote-only (default 12; must be >= micro). */
    uint32_t continuity_deadband;
    /* Looks-analog magnitude floor: both axes at/below => not analog (default 12). */
    uint32_t analog_min_mag;
    /* Per-axis magnitude at/above which sign conflicts break same-intent (default 8). */
    uint32_t same_intent_min_active;
    /* Predicted same-intent tolerance tail in significance (default 14). */
    uint32_t same_intent_tolerance;
    /* X facing-sign flip rewind threshold in significance (default 4). */
    uint32_t onset_facing_thresh;
    /* Per-axis delta that is always significant (default 40). */
    uint32_t onset_large_delta;
    /* Smash-class |x| threshold for the dash-gate X disagree proxy (default 56; <=0 disables). */
    int32_t dash_gate_min;
    /* Digital keyboard encoding: full deflection magnitude (default 85; 0 disables). */
    int8_t digital_axis_mag;
} RNetInputContractParams;

/*
 * Host gates: all callbacks optional (NULL = portable default = gate never fires).
 * Queried lazily — a gate is only called when every portable condition ahead of it
 * in the decision order already holds, so implementations may log / count / peek
 * rings freely without changing side-effect order vs an inline implementation.
 */
typedef struct RNetInputContractHostGates
{
    void *ctx;
    /* Equal frames, published predicted: pending host branch ticket forces rewind. */
    uint8_t (*equal_predicted_force_rewind)(void *ctx);
    /* Buttons-equal stick delta that cannot affect the hashed sim: promote wire
     * silently instead of opening an episode. */
    uint8_t (*absorb_stick_replace)(void *ctx);
    /* Completed-sim same-intent: host move-protect promote (may exceed deadbands). */
    uint8_t (*protect_promote)(void *ctx, int32_t dx, int32_t dy, uint32_t micro_deadband,
                               uint8_t old_predicted);
    /* Completed-sim same-intent: host blocks all deadband promotes (fragile aim window). */
    uint8_t (*block_deadband_promote)(void *ctx);
    /* Predicted row within continuity deadband: peer state watermark already agreed
     * past this tick (frame-commit / master-hash confirm). Fail closed when NULL. */
    uint8_t (*hash_confirm_promote)(void *ctx);
    /* Runway predicted onset-ahead: host defers the correction until wire settles. */
    uint8_t (*defer_predicted_correction)(void *ctx);
} RNetInputContractHostGates;

typedef enum RNetInputContractDecision
{
    nRNetInputContractRewind = 0,
    nRNetInputContractRewindEqualDeferred,
    nRNetInputContractPromoteEqual,
    nRNetInputContractPromoteAbsorb,
    nRNetInputContractPromoteProtect,
    nRNetInputContractPromoteMicro,
    nRNetInputContractPromoteContinuity,
    nRNetInputContractPromoteHashConfirm,
    nRNetInputContractPromoteDefer,
    nRNetInputContractPromoteInsignificant
} RNetInputContractDecision;

typedef enum RNetInputContractCorrectionClass
{
    nRNetInputContractClassButton = 0,
    nRNetInputContractClassRelease,
    nRNetInputContractClassOnsetFromZero,
    nRNetInputContractClassMicroStick,
    nRNetInputContractClassSameIntentContinuity,
    nRNetInputContractClassRealStick
} RNetInputContractCorrectionClass;

/* Fill defaults matching the frozen decision table (see docs/rollback.md). */
void rnet_input_contract_params_init_defaults(RNetInputContractParams *params);

/* 1 when the decision means "queue a rollback", 0 for every promote class. */
static inline uint8_t rnet_input_contract_decision_is_rewind(RNetInputContractDecision decision)
{
    return ((decision == nRNetInputContractRewind) ||
            (decision == nRNetInputContractRewindEqualDeferred))
               ? (uint8_t)1
               : (uint8_t)0;
}

uint8_t rnet_input_contract_stick_looks_analog(int8_t stick_x, int8_t stick_y,
                                               const RNetInputContractParams *params);
uint8_t rnet_input_contract_stick_same_analog_intent(int8_t ax, int8_t ay, int8_t bx, int8_t by,
                                                     const RNetInputContractParams *params);
uint8_t rnet_input_contract_stick_dash_gate_disagree_x(int8_t hold_x, int8_t wire_x,
                                                       const RNetInputContractParams *params);

/* Analog -> neutral, or clearly shedding magnitude (confirmed_deadband slack). */
uint8_t rnet_input_contract_stick_replace_is_release(const RNetInputContractFrame *old_frame,
                                                     const RNetInputContractFrame *wire,
                                                     const RNetInputContractParams *params);

/* Runway significance: buttons / onset-from-neutral / facing flip / large delta / deadband. */
uint8_t rnet_input_contract_correction_is_significant(const RNetInputContractFrame *old_frame,
                                                      const RNetInputContractFrame *new_frame,
                                                      uint8_t correction_is_predicted,
                                                      const RNetInputContractParams *params);

/* Telemetry classification of a queued correction (no host gates involved). */
RNetInputContractCorrectionClass
rnet_input_contract_classify_correction(const RNetInputContractFrame *old_frame,
                                        const RNetInputContractFrame *wire,
                                        const RNetInputContractParams *params);

/*
 * Master decision: published row vs authoritative/late wire row at one tick.
 * completed_sim: sim has already advanced past this tick (sim_now > tick).
 * published/wire must be non-NULL; gates may be NULL (all-portable defaults).
 */
RNetInputContractDecision
rnet_input_contract_stick_replace_decide(const RNetInputContractFrame *published,
                                         const RNetInputContractFrame *wire,
                                         uint8_t completed_sim,
                                         const RNetInputContractParams *params,
                                         const RNetInputContractHostGates *gates);

#ifdef __cplusplus
}
#endif

#endif /* RECOMP_NET_INPUT_CONTRACT_H */
