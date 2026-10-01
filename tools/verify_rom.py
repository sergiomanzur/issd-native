import hashlib
import sys
import os

EXPECTED_MD5 = "345ddedcd63412b9373dabb67c11fc05"
EXPECTED_SHA1 = "bf4da6c24748b2ac8270bde001a65ad377b8d78d"
EXPECTED_SHA256 = "d2fe66c1ce66c65ce14e478c94be2e616f9e2cad374b5783a6a64d3c1a99cfa9"
EXPECTED_SIZE = 2097152  # 2MB (2,097,152 bytes)

def verify_rom(path):
    if not os.path.exists(path):
        print(f"[ERROR] ROM file not found at: {path}")
        return False

    with open(path, "rb") as f:
        data = f.read()

    size = len(data)
    md5_hash = hashlib.md5(data).hexdigest().lower()
    sha1_hash = hashlib.sha1(data).hexdigest().lower()
    sha256_hash = hashlib.sha256(data).hexdigest().lower()

    print(f"File: {path}")
    print(f"Size: {size} bytes (Expected: {EXPECTED_SIZE})")
    print(f"MD5:    {md5_hash}")
    print(f"SHA1:   {sha1_hash}")
    print(f"SHA256: {sha256_hash}")

    if size != EXPECTED_SIZE:
        print(f"[WARNING] Size mismatch! Expected 2MB headerless ROM.")

    if md5_hash == EXPECTED_MD5:
        print("[SUCCESS] Verified clean USA Headerless ISSD ROM!")
        return True
    else:
        print(f"[ERROR] MD5 mismatch! Expected {EXPECTED_MD5}")
        return False

if __name__ == "__main__":
    rom_path = sys.argv[1] if len(sys.argv) > 1 else "International Superstar Soccer Deluxe (USA).sfc"
    success = verify_rom(rom_path)
    sys.exit(0 if success else 1)
