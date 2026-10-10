from itertools import count
from types import SimpleNamespace
from pathlib import Path
import sys
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'deps/snesrecomp/recompiler'))
from snes65816 import IMM
from v2.lowering import lower
from v2.ir import Read, ConstI, SegKind, Value
from v2.cfg_loader import load_bank_cfg
from v2.codegen import emit_op
from v2.emit_function import emit_function


@pytest.mark.parametrize('pc', [0x038b27, 0x838b27])
@pytest.mark.parametrize('narrow', [0, 1])
def test_selected_operand_reads_its_program_bank(pc, narrow):
    instruction = SimpleNamespace(addr=pc, mode=IMM, mnem='CMP', operand=0x708,
                                  m_flag=narrow, x_flag=0)
    ids = count()
    ops = lower(instruction, value_factory=lambda: Value(next(ids)),
                runtime_immediates=frozenset({0x838b27}))
    assert isinstance(ops[0], Read)
    assert ops[0].seg.kind == SegKind.LONG
    assert ops[0].seg.bank == pc >> 16
    assert ops[0].seg.offset == (pc + 1) & 0xffff
    assert ops[0].width == (1 if narrow else 2)
    emitted = '\n'.join(emit_op(ops[0]))
    assert f'{pc >> 16:#04x}' in emitted
    assert 'cpu->DB' not in emitted


def test_unselected_operand_remains_constant():
    instruction = SimpleNamespace(addr=0x838b27, mode=IMM, mnem='CMP', operand=0x708,
                                  m_flag=0, x_flag=0)
    ids = count()
    ops = lower(instruction, value_factory=lambda: Value(next(ids)))
    assert isinstance(ops[0], ConstI) and ops[0].value == 0x708


def test_cfg_directive_normalizes_mirrors_and_rejects_duplicates(tmp_path):
    path = tmp_path / 'bank03.cfg'
    path.write_text('bank = 03\nruntime_immediate 838B27\n')
    assert load_bank_cfg(path).runtime_immediates == {0x038b27}
    path.write_text('bank = 03\nruntime_immediate 838B27\n'
                    'runtime_immediate 038B27\n')
    with pytest.raises(ValueError, match='duplicate'):
        load_bank_cfg(path)


def test_instruction_hook_runs_inside_a_native_block(tmp_path):
    path = tmp_path / 'bank00.cfg'
    path.write_text('bank = 00\nopcode_hook 808003\n')
    cfg = load_bank_cfg(path)
    assert cfg.opcode_hooks == {0x008003}
    rom = bytes([0xc9, 1, 0, 0xc9, 2, 0, 0x6b]) + bytes(0x8000 - 7)
    source = emit_function(rom, 0, 0x8000, 0, 0,
                           opcode_hooks=cfg.opcode_hooks)
    assert 'g_cpu_native_block_hook(cpu, 0x008003)' in source
