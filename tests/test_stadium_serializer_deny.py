"""The patched logical-ID store must be read through the cartridge interpreter."""
import re
from pathlib import Path


def test_stadium_serializer_compiled_owner_is_denied():
    root = Path(__file__).resolve().parents[1]
    source = (root / "recomp/generated/bank05_part04_v2.c").read_text()
    store = source.index("cpu_write16(cpu, cpu->DB, (uint16)(0x1fa2), _v100)")
    owners = list(re.finditer(r"RecompReturn (\w+)\(CpuState \*cpu\) \{", source[:store]))
    owner = owners[-1].group(1)
    assert owner == "CODE_85A3FB_M0X0"
    dispatch = (root / "recomp/generated/dispatch_v2.c").read_text()
    entry = re.search(r"\{ 0x([0-9A-Fa-f]+)u, \{ " + owner, dispatch)
    assert entry, "the actual compiled serializer entry must be located"
    pc = int(entry.group(1), 16)
    denied = {int(line.strip(), 16) for line in
              (root / "recomp/aot_boot_deny.txt").read_text().splitlines()
              if line.strip() and not line.lstrip().startswith("#")}
    assert pc in denied and (pc | 0x800000) in denied
