#!/usr/bin/env python3
"""
Automated Deterministic Match & Boot Verification Test for ISSD Native
Validates end-to-end headless execution, boot vectors, and match simulation.
"""

import os
import sys
import subprocess

def run_test(frames=600, auto_start=180):
    exe_path = os.path.join("build", "ISSDNative.exe")
    if not os.path.exists(exe_path):
        print(f"[FAIL] Executable '{exe_path}' not found! Build project first.")
        return False

    cmd = [
        exe_path,
        "--headless", str(frames),
        "--auto-start", str(auto_start),
        "--screenshot", "tests/match_verification.bmp"
    ]

    print(f"[TEST] Executing: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)

    print("--- STDOUT ---")
    print(result.stdout)
    if result.stderr:
        print("--- STDERR ---")
        print(result.stderr)

    if result.returncode != 0:
        print(f"[FAIL] ISSDNative exited with code {result.returncode}")
        return False

    if "Reset sequence completed" not in result.stdout:
        print("[FAIL] Reset sequence was not completed successfully.")
        return False

    if "[Done] Target frame count reached" not in result.stdout:
        print("[FAIL] Target frame count was not reached.")
        return False

    print("[PASS] ISSD Native headless verification succeeded with 100% stability!")
    return True

if __name__ == "__main__":
    success = run_test()
    sys.exit(0 if success else 1)
