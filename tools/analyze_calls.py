import re
import os

asm_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm"
with open(asm_path, "r", encoding="latin-1") as f:
    content = f.read()

# Split banks
bank_splits = re.split(r'macro\s+ISSDBank([0-9A-Fa-f]{2})Macros', content)

# Collect all JSL / JSR targets across the entire codebase
jsl_targets = set(re.findall(r'JSL[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))
jsr_targets = set(re.findall(r'JSR[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))
jmp_targets = set(re.findall(r'JMP[.\w]*\s+CODE_([0-9A-Fa-f]{6})', content))

print(f"Total JSL targets: {len(jsl_targets)}")
print(f"Total JSR targets: {len(jsr_targets)}")
print(f"Total JMP targets: {len(jmp_targets)}")

# Print some bank-specific statistics
for i in range(1, len(bank_splits), 2):
    bank_hex = bank_splits[i]
    body = bank_splits[i+1]
    bank_int = int(bank_hex, 16) & 0x3F
    
    labels = re.findall(r'CODE_([0-9A-Fa-f]{6}):', body)
    # Entry points in this bank that are targeted by JSL or JSR or are reset/nmi
    bank_jsl = [addr for addr in labels if addr in jsl_targets]
    bank_jsr = [addr for addr in labels if addr in jsr_targets]
    
    if len(labels) > 0:
        print(f"Bank  (index {bank_int:02d}): total labels={len(labels)}, JSL targets={len(bank_jsl)}, JSR targets={len(bank_jsr)}")
