import re
import os
import sys

asm_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm"
ram_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/RAM_Map_ISSD.asm"
out_cfg_dir = r"recomp/config"

os.makedirs(out_cfg_dir, exist_ok=True)

with open(asm_path, "r", encoding="latin-1") as f:
    asm_content = f.read()

with open(ram_path, "r", encoding="latin-1") as f:
    ram_content = f.read()

# Parse RAM map
ram_symbols = {}
for line in ram_content.splitlines():
    line = line.strip()
    m = re.match(r'!(\w+)\s*=\s*\$([0-9A-Fa-f]+)', line)
    if m:
        ram_symbols[int(m.group(2), 16)] = m.group(1)

print(f"Extracted {len(ram_symbols)} RAM symbols.")

# Parse RAM map into docs/RAM_MAP.md
with open("docs/RAM_MAP.md", "w", encoding="utf-8") as f:
    f.write("# ISSD RAM Map\n\n")
    f.write("| Address | Variable / Identifier | Notes |\n")
    f.write("| --- | --- | --- |\n")
    for addr in sorted(ram_symbols.keys()):
        name = ram_symbols[addr]
        f.write(f"| `${addr:06X}` | `{name}` | [CONFIRMED] from disassembly |\n")

print("Wrote docs/RAM_MAP.md")

# Find cross-bank call targets across the entire ASM codebase.
all_jsl_targets = set(re.findall(r'JSL[.\w]*\s+CODE_([0-9A-Fa-f]{6})', asm_content))

# Split by bank macros
bank_splits = re.split(r'macro\s+ISSDBank([0-9A-Fa-f]{2})Macros', asm_content)

total_funcs_declared = 0
total_symbols_declared = 0

routine_map_entries = []

for i in range(1, len(bank_splits), 2):
    bank_hex = bank_splits[i]
    body = bank_splits[i+1]
    
    bank_snes = int(bank_hex, 16)
    lorom_bank_idx = bank_snes & 0x3F
    
    cfg_filename = f"bank{lorom_bank_idx:02x}.cfg"
    cfg_path = os.path.join(out_cfg_dir, cfg_filename)
    
    lines = []
    lines.append(f"bank = {lorom_bank_idx:02x}")
    
    if lorom_bank_idx == 0:
        lines.append("auto_vectors")
        lines.append("tier_down_stubs")
    
    # Extract code labels in this bank
    raw_labels = re.findall(r'(CODE_([0-9A-Fa-f]{6})):', body)
    
    # Identify externally reachable entry points. Same-bank JSR targets are
    # labels, not unconditional reset-width roots: v2_regen discovers and
    # promotes them at the exact M/X mode of each reachable call site.
    seen_addrs = set()
    
    # In LoROM, bank addresses in this bank start at 0x8000
    for full_name, addr_hex in raw_labels:
        addr24 = int(addr_hex, 16)
        addr16 = addr24 & 0xFFFF
        
        if addr16 in seen_addrs:
            continue
        seen_addrs.add(addr16)
        
        # JSL targets and vectors are roots. JSR-only targets remain symbols;
        # promoting every one as M1X1 also emits unreachable disassembly labels
        # and decodes real routines at the wrong immediate-operand width.
        is_jsl = addr_hex in all_jsl_targets
        is_vector = (addr24 in (0x808000, 0x8080E0, 0x8081A4, 0x80FF90, 0x80FF9B, 0x80FF97))
        
        if is_jsl or is_vector:
            lines.append(f"func {full_name} {addr16:04x}")
            total_funcs_declared += 1
            routine_map_entries.append((addr24, full_name, "Function Entry (JSL/Vector)"))
        else:
            lines.append(f"symbol {addr16:04x} {full_name}")
            total_symbols_declared += 1
            routine_map_entries.append((addr24, full_name, "Internal Label"))
            
    with open(cfg_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

print(f"Generated 64 bank configs in {out_cfg_dir}")
print(f"Total funcs declared: {total_funcs_declared}, total symbols: {total_symbols_declared}")

# Write funcs.h
with open(os.path.join(out_cfg_dir, "funcs.h"), "w", encoding="utf-8") as f:
    f.write("""/* Declarations for generated C. */
#pragma once
#include "cpu_state.h"
""")

# Run jump table extraction and populate rich docs/ROUTINE_MAP.md
print("Extracting jump tables and updating bank configs with indirect_dispatch directives...")
import subprocess
subprocess.run([sys.executable, "tools/extract_jump_tables.py"], check=True)

