from pathlib import Path
from tools.promote_stadium_codegen import audit


def test_only_equivalent_regenerated_functions_can_be_promoted(tmp_path):
    old, new = tmp_path / 'old', tmp_path / 'new'
    old.mkdir()
    new.mkdir()
    original = ('RecompReturn bank_03_8AFF_M0X0(CpuState *cpu) {\n'
                '    uint16 _v35 = 0x708;\n'
                '    cpu->cycles += 3;\n'
                '    return 0;\n}\n')
    generated = original.replace('0x708', 'cpu_read16(cpu, 0x03, (uint16)(0x8b28))')
    (old / 'bank03_part01_v2.c').write_text(original)
    (new / 'bank03_part01_v2.c').write_text(generated)
    rom = bytearray(0x20000)
    rom[0x18b28:0x18b2a] = b'\x08\x07'
    report, edits = audit(old, new, rom)
    assert len(report) == 1 and report[0]['eligible']
    assert edits[old / 'bank03_part01_v2.c'] == generated
    (new / 'bank03_part01_v2.c').write_text(generated.replace('+= 3', '+= 4'))
    report, edits = audit(old, new, rom)
    assert not report[0]['eligible'] and not edits
