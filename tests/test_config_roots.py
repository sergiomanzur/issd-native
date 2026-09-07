"""Project config roots must represent externally reachable ROM entries."""

import pathlib
import re


REPO = pathlib.Path(__file__).resolve().parents[1]
ASM = (REPO / 'deps' / 'ISSD-disassembly' /
       'International_Superstar_Soccer_Deluxe' / 'Routine_Macros_ISSD.asm')
CFG_DIR = REPO / 'recomp' / 'config'
VECTORS = {
    0x808000, 0x8080E0, 0x8081A4, 0x80FF90, 0x80FF97, 0x80FF9B,
}


def test_only_cross_bank_entries_and_vectors_are_canonical_roots():
    asm = ASM.read_text(encoding='latin-1')
    jsl_targets = {
        int(x, 16) for x in
        re.findall(r'JSL[.\w]*\s+CODE_([0-9A-Fa-f]{6})', asm)
    }
    jsr_targets = {
        int(x, 16) for x in
        re.findall(r'JSR[.\w]*\s+CODE_([0-9A-Fa-f]{6})', asm)
    }

    funcs = set()
    symbols = set()
    for cfg_path in CFG_DIR.glob('bank*.cfg'):
        text = cfg_path.read_text(encoding='utf-8')
        funcs.update(int(x, 16) for x in re.findall(
            r'^func\s+CODE_([0-9A-Fa-f]{6})\s', text, re.MULTILINE))
        symbols.update(int(x, 16) for x in re.findall(
            r'^symbol\s+[0-9A-Fa-f]+\s+CODE_([0-9A-Fa-f]{6})$',
            text, re.MULTILINE))

    unexpected_roots = funcs - jsl_targets - VECTORS
    assert not unexpected_roots, (
        f'{len(unexpected_roots)} JSR/internal labels are unconditional '
        f'M1X1 roots; examples: {sorted(unexpected_roots)[:12]}')

    jsr_only = jsr_targets - jsl_targets - VECTORS
    assert jsr_only, 'fixture must include JSR-only labels'
    assert jsr_only <= symbols, (
        f'{len(jsr_only - symbols)} JSR-only labels are missing symbol '
        f'annotations; examples: {sorted(jsr_only - symbols)[:12]}')
