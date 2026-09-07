#include <stdio.h>
#include <string.h>
#include "cpu_state.h"

#define RECOMP_STACK_DEPTH 64
int g_recomp_stack_top;
uint16_t g_cpu_entry_s[RECOMP_STACK_DEPTH];
static uint16_t inherited_entry;
static int remaining_tails;
static int nonlocal_return;

/* The test driver inserts the exact production function, not a duplicate of
 * its distance calculation. Slots model the generated RecompStack entries. */
/* ACTUAL_RUNTIME_RESOLVER */

void RecompStackPop(void) { g_recomp_stack_top--; }
void cpu_tailcall_inherit_return_context(uint16_t entry_s, uint8_t hrv) {
    (void)hrv;
    inherited_entry = entry_s;
}

static RecompReturn tail_owner(CpuState *cpu) {
    uint16_t _entry_s = g_cpu_entry_s[g_recomp_stack_top - 1];
    uint8_t _hrv = 2;
    /* ACTUAL_EMITTED_TAIL */
}

RecompReturn TailTarget_M1X1(CpuState *cpu) {
    g_cpu_entry_s[g_recomp_stack_top++] = inherited_entry;
    if (--remaining_tails > 0)
        return tail_owner(cpu);  /* another split suffix, same guest frame */
    RecompReturn result = RECOMP_RETURN_NORMAL;
    if (nonlocal_return) {
        /* PLA; RTS consumed tail owner's JSR and caller's JSR. */
        cpu->S = 0x0199;
        result = (RecompReturn)cpu_resolve_ancestor_skip(0x0197);
    } else {
        cpu->S = 0x0197;  /* ordinary RTS consumes only its inherited frame */
    }
    RecompStackPop();
    return result;
}

int main(void) {
    int failures = 0;
    for (int count = 1; count <= 3; count++) {
        for (nonlocal_return = 0; nonlocal_return <= 1; nonlocal_return++) {
            CpuState cpu;
            memset(&cpu, 0, sizeof cpu);
            cpu.S = 0x0195;
            /* Root owns a JSL; caller and tail owner each own a JSR. */
            g_cpu_entry_s[0] = 0x0199;
            g_cpu_entry_s[1] = 0x0197;
            g_cpu_entry_s[2] = 0x0195;
            g_recomp_stack_top = 3;
            remaining_tails = count;
            RecompReturn result = tail_owner(&cpu);
            int expected = nonlocal_return ? RECOMP_RETURN_SKIP_1 : RECOMP_RETURN_NORMAL;
            if ((int)result != expected || g_recomp_stack_top != 2) {
                fprintf(stderr, "tails=%d nonlocal=%d: return=%d expected=%d depth=%d expected=2\n",
                        count, nonlocal_return, result, expected, g_recomp_stack_top);
                failures++;
            }
            /* Generated caller's ordinary call propagation consumes one
             * remaining skip. NORMAL executes its own RTS before returning. */
            if (result) result = (RecompReturn)((int)result - 1);
            else cpu.S += 2;
            RecompStackPop();
            if (result != RECOMP_RETURN_NORMAL) {
                fprintf(stderr, "tails=%d: root was incorrectly skipped\n", count);
                failures++;
            } else {
                cpu.S += 3;  /* root resumes and executes its RTL */
            }
            RecompStackPop();
            if (cpu.S != 0x019C || g_recomp_stack_top != 0) {
                fprintf(stderr, "tails=%d nonlocal=%d: final S=%04X expected=019C\n",
                        count, nonlocal_return, cpu.S);
                failures++;
            }
        }
    }
    printf("6 tail-chain scenarios, %d failures\n", failures);
    return failures ? 1 : 0;
}
