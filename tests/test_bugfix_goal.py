from pathlib import Path
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_goal_boundary_policy(tmp_path):
    repo = Path(__file__).resolve().parents[1]
    exe = tmp_path / "bugfix_goal.exe"
    compile_c(exe, repo, [repo / "tests/test_bugfix_goal.c",
                        repo / "ISSDNative/issd_bugfix_goal.c"], [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_original_generated_goal_and_score_routines(tmp_path):
    """Execute exact generated cartridge decisions with real RAM and registers."""
    repo = Path(__file__).resolve().parents[1]
    goal = (repo / "recomp/generated/bank03_part01_v2.c").read_text()
    score = (repo / "recomp/generated/bank06_v2.c").read_text()
    restart = (repo / "recomp/generated/bank24_part0b_v2.c").read_text()
    fixture = (repo / "tests/test_bugfix_goal.c").read_text().split("int main(void)")[0]
    source = fixture + r'''
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int RecompReturn;
#define RECOMP_RETURN_NORMAL 0
#define BD_EXIT_KIND_TRAMPOLINE 0
typedef struct { uint16 A,X,Y,D,S; uint8 P,PB,DB,host_return_valid;
 int _flag_C,_flag_N,_flag_Z,_flag_V,_flag_D;
 uint64_t cycles,master_cycles,coprocessor_master_cycles; } CpuState;
const char *g_last_recomp_func;
int g_recomp_stack_top;
uint16 g_cpu_entry_s[1];
static bool fixed;
#define RecompStackPush(...) ((void)0)
#define RecompStackPop(...) ((void)0)
#define WatchdogCheck(...) ((void)0)
#define cpu_dbg_funcname(...) ((void)0)
#define cpu_trace_func_entry(...) ((void)0)
#define cpu_trace_mark_nlr_exit(...) ((void)0)
#define cpu_trace_stack_op(...) ((void)0)
#define interp_bridge_lle_master_deadline_reached(...) 0
#define interp_bridge_lle_yield_unwind(...) 0
#define cpu_take_tailcall_return_context(...) 0
#define cpu_resolve_ancestor_skip(...) -1
#define interp_bridge_return_targets_owner(...) 0
#define interp_bridge_has_direct_paired_bounce(...) 0
#define cpu_dispatch_has_entry(...) 0
#define interp_tier_dispatch_rewritten_return(...) 0
#define interp_tier_dispatch_popped_return(...) 0
#define cpu_dispatch_pc_from(...) 0
#define cpu_read8(c,b,a) (ram[(uint16)(a)])
#define cpu_read16(c,b,a) word((uint16)(a))
#define cpu_write16(c,b,a,v) put((uint16)(a),(v))
#define cpu_read_a16(c) ((c)->A)
#define cpu_read_x16(c) ((c)->X)
#define cpu_read_y16(c) ((c)->Y)
#define cpu_write_a_m(c,v) ((c)->A=(v))
#define cpu_write_x_x(c,v) ((c)->X=(v))
#define cpu_write_y_x(c,v) ((c)->Y=(v))
static void cpu_trace_block(CpuState *cpu, uint32_t pc) {
 (void)cpu;
 if(pc==0x06dbbe && fixed) issd_bugfix_goal_score(ram,(uint16)word(0x14d2),true);
 if(pc==0x24dbf6 && fixed) issd_bugfix_goal_restart(ram,true);
}
'''
    source += function(goal, "RecompReturn bank_03_8D61_M0X0(CpuState *cpu) {")
    source += function(score, "RecompReturn bank_06_DB79_M0X0(CpuState *cpu) {")
    # Exact original coordinate-copy and quadrant blocks. Stop before external
    # restart dispatch; no game logic is replaced or simulated within them.
    start = restart.index("  L_DBDB_M0X0:")
    end = restart.index("  L_DBFF_M0X0:", start)
    source += "static RecompReturn restart_original(CpuState *cpu) {\n" + restart[start:end]
    source += "  L_DBFF_M0X0: return 0;\n}\n"
    source += r'''
static int award(int previous, int current, bool enabled) {
 witness(previous,current);
 CpuState cpu={0}; cpu.X=0x400; cpu.S=0x1ed; cpu.PB=3;
 cpu.host_return_valid=2; put(cpu.S+1,0x8daa);
 assert(bank_03_8D61_M0X0(&cpu)==0);
 int original=cpu._flag_C;
 if(!original && issd_bugfix_goal_reject(ram,enabled)) cpu._flag_C=1;
 return !cpu._flag_C;
}
int main(void) {
 (void)before;
 /* Exact original routine approves the same behind-goal ball repeatedly. */
 assert(award(-8,-5,false)); assert(award(-8,-5,false));
 assert(!award(-8,-5,true));
 assert(award(0x706,0x704,false)); assert(!award(0x706,0x704,true));
 assert(award(-4,-5,true)); assert(award(0x703,0x704,true));
 assert(!award(100,100,true));
 for(int side=0;side<2;side++) {
   witness(0,side?0x704:-5); put(0xda4,0x8000); put(0x45c,0x600);
   unsigned team=side?0xe00:0xd00; put(0x69a,team); put(team+0xa2,99);
   CpuState cpu={0}; cpu.S=0x1ed; cpu.PB=6; cpu.host_return_valid=2;
   put(cpu.S+1,0xd9a9); /* JSR return frame */
   fixed=false; assert(bank_06_DB79_M0X0(&cpu)==0); assert(word(team+0xa2)==100);
   put(team+0xa2,99); cpu.S=0x1ed; put(cpu.S+1,0xd9a9);
   fixed=true; assert(bank_06_DB79_M0X0(&cpu)==0); assert(word(team+0xa2)==99);
 }
 for(int side=0;side<2;side++) {
   witness(0,0); put(0xbc,3); put(0x12f2,0x380); put(0x12d8,0xe0);
   unsigned spot=side?0x708:0xfff8;
   put(0x62a,spot); put(0x62c,0xe0);
   CpuState cpu={0}; cpu.X=0x600;
   fixed=false; assert(restart_original(&cpu)==0);
   assert(word(0xca)==spot && word(0x11f0)==spot); /* Cartridge publishes invalid spot. */
   fixed=true; assert(restart_original(&cpu)==0);
   assert(word(0xca)==(side?0x6fc:4) && word(0x11f0)==word(0xca));
   assert(word(0xce)==0xe0 && word(0x11f2)==0xe0 && word(0xbe)==(side?3:1));
   put(0x62a,600); put(0x62c,120); fixed=false; restart_original(&cpu);
   memcpy(before,ram,sizeof ram); fixed=true; restart_original(&cpu);
   assert(!memcmp(before,ram,sizeof ram)); /* Normal foul coordinates and quadrant identical. */
 }
 puts("original generated goal and score witnesses passed"); return 0;
}
'''
    path = tmp_path / "original_goal.c"
    path.write_text(source)
    exe = tmp_path / "original_goal.exe"
    compile_c(exe, repo, [path, repo / "ISSDNative/issd_bugfix_goal.c"], [repo / "ISSDNative"])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
