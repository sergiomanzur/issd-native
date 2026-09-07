"""Structural AOT gaps tier the affected variant to the interpreter."""

import pathlib
import sys
import types


REPO = pathlib.Path(__file__).resolve().parents[2]
if str(REPO / 'tools') not in sys.path:
    sys.path.insert(0, str(REPO / 'tools'))

import v2_regen  # noqa: E402


def _cfg():
    entry = types.SimpleNamespace(
        name='Broken', start=0x8123, entry_m=1, entry_x=1)
    return types.SimpleNamespace(entries=[entry])


def test_structural_gap_replaces_only_affected_variant_body():
    source = '''/* preamble */
RecompReturn Broken_M1X1(CpuState *cpu) {
  /* Call indirect SUPPRESSED: no table */
  return RECOMP_RETURN_NORMAL;
}
RecompReturn Broken(CpuState *cpu) { return Broken_M1X1(cpu); }
RecompReturn Good_M1X1(CpuState *cpu) {
  return RECOMP_RETURN_NORMAL;
}
'''

    rewritten, tiered = v2_regen._tier_down_structural_variants(
        source, 0x00, _cfg())

    assert tiered == {(0x008123, 1, 1)}
    assert 'Call indirect SUPPRESSED' not in rewritten
    assert 'interp_tier_dispatch_balanced(cpu, 0x008123u' in rewritten
    assert 'RecompReturn Broken(CpuState *cpu)' in rewritten
    assert 'RecompReturn Good_M1X1(CpuState *cpu)' in rewritten


def test_unlowered_software_interrupt_remains_a_hard_lint_failure():
    source = '''RecompReturn Broken_M1X1(CpuState *cpu) {
  /* BRK: software interrupt */
  return RECOMP_RETURN_NORMAL;
}
'''

    rewritten, tiered = v2_regen._tier_down_structural_variants(
        source, 0x00, _cfg())

    assert rewritten == source
    assert not tiered
