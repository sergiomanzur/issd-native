import re
import os
import sys

asm_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm"
with open(asm_path, "r", encoding="latin-1") as f:
    content = f.read()

# 1. Collect all explicit call targets
jsl_targets = set(re.findall(r'JSL[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))
jsr_targets = set(re.findall(r'JSR[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))
jmp_targets = set(re.findall(r'JMP[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))
branch_targets = set(re.findall(r'B[A-Za-z]{2}[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))

# 2. Collect all jump table targets from tools/extract_jump_tables.py / config files
jt_targets = set()
for cfg in os.listdir("recomp/config"):
    if cfg.endswith(".cfg"):
        with open(os.path.join("recomp/config", cfg), "r", encoding="utf-8") as f:
            for line in f:
                # check for indirect_dispatch or target comments
                m = re.findall(r'CODE_([0-9A-Fa-f]{6})', line)
                for tgt in m:
                    jt_targets.add(tgt)

print(f"JSL targets: {len(jsl_targets)}")
print(f"JSR targets: {len(jsr_targets)}")
print(f"JMP targets: {len(jmp_targets)}")
print(f"Branch targets: {len(branch_targets)}")
print(f"Jump Table / Config targets: {len(jt_targets)}")

all_referenced = jsl_targets | jsr_targets | jmp_targets | branch_targets | jt_targets
print(f"Total uniquely referenced code labels: {len(all_referenced)}")

# 3. Find all subroutines (blocks starting with CODE_ and ending with RTS/RTL)
subroutine_blocks = re.findall(r'(CODE_([0-9A-Fa-f]{6}):(.*?)(?:^\s+(?:RTS|RTL)\b))', content, re.M | re.DOTALL)
print(f"Total subroutine blocks with RTS/RTL: {len(subroutine_blocks)}")

referenced_subroutines = []
unreferenced_subroutines = []

for full_match, addr_hex, body in subroutine_blocks:
    if addr_hex in all_referenced:
        referenced_subroutines.append((addr_hex, body))
    else:
        unreferenced_subroutines.append((addr_hex, body))

print(f"Referenced subroutines (reachable via call/branch/jump table): {len(referenced_subroutines)}")
print(f"Unreferenced subroutines: {len(unreferenced_subroutines)}")

# 4. Analyze unreferenced subroutines: are they valid code or data tables?
# Characteristics of valid 65816 code: has standard instructions (LDA, STA, LDX, STX, CMP, etc.)
# Characteristics of data misinterpretations: db/dw/dd, invalid opcodes, or repetitive sequences
code_subroutines = []
data_misinterpretations = []

valid_opcodes = {
    "LDA", "STA", "LDX", "STX", "LDY", "STY", "STZ", "TAX", "TAY", "TXA", "TYA",
    "TSX", "TXS", "TXY", "TYX", "PHA", "PLA", "PHP", "PLP", "PHX", "PLX", "PHY", "PLY",
    "PHB", "PLB", "PHD", "PLD", "PHK", "ADC", "SBC", "CMP", "CPX", "CPY", "INC", "DEC",
    "INX", "DEX", "INY", "DEY", "ASL", "LSR", "ROL", "ROR", "AND", "ORA", "EOR", "BIT",
    "CLC", "SEC", "CLI", "SEI", "CLD", "SED", "CLV", "REP", "SEP", "XBA", "XCE",
    "BRA", "BEQ", "BNE", "BMI", "BPL", "BCC", "BCS", "BVC", "BVS", "JSR", "JSL", "RTS", "RTL"
}

for addr_hex, body in unreferenced_subroutines:
    lines = [l.strip() for l in body.splitlines() if l.strip() and not l.strip().startswith(";")]
    if not lines:
        continue
    # Extract mnemonics
    mnemonics = []
    has_db_dw = False
    for line in lines:
        if line.startswith("db ") or line.startswith("dw ") or line.startswith("dd ") or line.startswith("dl "):
            has_db_dw = True
            break
        # first token
        m = re.match(r"^([A-Za-z]{3})[.\s]", line)
        if m:
            mnemonics.append(m.group(1).upper())
    
    if has_db_dw or not mnemonics:
        data_misinterpretations.append((addr_hex, "Contains db/dw data"))
    else:
        # Check ratio of valid 65816 mnemonics
        valid_count = sum(1 for op in mnemonics if op in valid_opcodes)
        ratio = valid_count / len(mnemonics)
        if ratio >= 0.8:
            code_subroutines.append((addr_hex, mnemonics[:5]))
        else:
            data_misinterpretations.append((addr_hex, f"Low valid opcode ratio: {ratio:.2f}"))

print(f"\n--- Analysis of {len(unreferenced_subroutines)} Unreferenced Disassembly Blocks ---")
print(f"Confirmed Embedded Data / Misinterpreted Tables: {len(data_misinterpretations)}")
print(f"Dormant/Dead 65816 Code Subroutines: {len(code_subroutines)}")

print("\nSample dormant 65816 subroutines (uncalled in retail USA):")
for addr_hex, sample_ops in code_subroutines[:10]:
    print(f"  CODE_{addr_hex}: instructions={sample_ops}")
