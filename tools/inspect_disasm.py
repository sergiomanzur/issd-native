import re
import os
import sys

asm_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm"
ram_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/RAM_Map_ISSD.asm"

print(f"Checking disassembly file: {asm_path}")
if not os.path.exists(asm_path):
    print("Error: ASM file not found")
    sys.exit(1)

with open(asm_path, "r", encoding="latin-1") as f:
    content = f.read()

# Find all bank macros: macro ISSDBank([0-9A-F]{2})Macros
bank_macros = list(re.finditer(r'macro\s+ISSDBank([0-9A-Fa-f]{2})Macros', content))
print(f"Found {len(bank_macros)} bank macros.")

# Find all CODE_ and DATA_ labels
code_labels = set(re.findall(r'\b(CODE_([0-9A-Fa-f]{6}))\b', content))
data_labels = set(re.findall(r'\b(DATA_([0-9A-Fa-f]{6}))\b', content))
print(f"Found {len(code_labels)} unique CODE_ labels and {len(data_labels)} unique DATA_ labels.")

# Find indirect jumps: JMP/JSR/JML (,X) or () or []
indirect_jmps = list(re.finditer(r'(\bCODE_[0-9A-Fa-f]{6}\b[^\n]*\n(?:[^\n]*\n){0,10}?)\s*(JMP|JSR|JML)\b[.\w]*\s*([(\[][^)\n]+[)\]])', content))
print(f"Found {len(indirect_jmps)} indirect jump occurrences.")

