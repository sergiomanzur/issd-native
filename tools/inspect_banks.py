import re

asm_path = r"deps/ISSD-disassembly/International_Superstar_Soccer_Deluxe/Routine_Macros_ISSD.asm"
with open(asm_path, "r", encoding="latin-1") as f:
    content = f.read()

# Split by bank macros
bank_splits = re.split(r'macro\s+ISSDBank([0-9A-Fa-f]{2})Macros', content)
print(f"Split produced {len(bank_splits)} parts.")

# Header is part 0, then (bank_hex, bank_body) pairs
for i in range(1, len(bank_splits), 2):
    bank_hex = bank_splits[i]
    body = bank_splits[i+1]
    
    # Extract labels in this bank
    labels = re.findall(r'(CODE_([0-9A-Fa-f]{6})):', body)
    data_labels = re.findall(r'(DATA_([0-9A-Fa-f]{6})):', body)
    
    # Check JMP / JSR indirect
    indirects = re.findall(r'(JMP|JSR|JML)\s+([(\[][^)\n]+[)\]])', body)
    
    bank_int = int(bank_hex, 16) & 0x3F
    print(f"Bank  (LoROM bank index {bank_int:02X}): {len(labels)} code labels, {len(data_labels)} data labels, {len(indirects)} indirect jumps")
    if i == 1:
        # Show first 5 code labels
        print(f"  Sample code labels: {labels[:5]}")
