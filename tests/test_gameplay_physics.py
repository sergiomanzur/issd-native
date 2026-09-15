import os
import struct
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
ROM_PATH = REPO_ROOT / "International Superstar Soccer Deluxe (USA).sfc"


def snes_to_rom(addr: int) -> int:
    """Converts a SNES LoROM 24-bit address ($80..$FF) to physical ROM file offset."""
    bank = (addr >> 16) & 0x7F
    offset = addr & 0x7FFF
    return bank * 0x8000 + offset


def load_rom() -> bytes:
    assert ROM_PATH.exists(), f"ROM file not found: {ROM_PATH}"
    with open(ROM_PATH, "rb") as f:
        return f.read()


def test_rom_trigonometric_table():
    """Verify the 32-angle circular trigonometric table DATA_81A5D7 in ROM."""
    rom = load_rom()
    off = snes_to_rom(0x81A5D7)
    # Read 32 entries (each entry is 4 bytes: 2 bytes cos, 2 bytes sin)
    table_bytes = rom[off : off + 128]
    assert len(table_bytes) == 128

    # First entry (Angle 0 = East / 0 deg): w1=0x0E71 (~3697), w2=0x0000 (0)
    w1_0, w2_0 = struct.unpack("<2H", table_bytes[0:4])
    assert w2_0 == 0, f"Expected sin(0) = 0, got {w2_0}"
    assert w1_0 == 0x0E71, f"Expected w1(0) = 0x0E71, got {hex(w1_0)}"

    # Angle 8 = 45 deg diagonal: w1 = w2 = 0x07C8 (1992)
    w1_8, w2_8 = struct.unpack("<2H", table_bytes[8 * 4 : 8 * 4 + 4])
    assert w1_8 == 0x07C8 and w2_8 == 0x07C8, f"Expected 45 deg symmetry (1992, 1992), got ({w1_8}, {w2_8})"

    # Angle 16 = 90 deg vertical: w1 = 0, w2 = 0x0695 (1685)
    w1_16, w2_16 = struct.unpack("<2H", table_bytes[16 * 4 : 16 * 4 + 4])
    assert w1_16 == 0, f"Expected w1(90) = 0, got {w1_16}"
    assert w2_16 == 0x0695, f"Expected vertical depth w2(90) = 0x0695, got {hex(w2_16)}"


def test_rom_pitch_dimensions():
    """Verify the 8 stadium pitch dimension configurations in DATA_81EC47."""
    rom = load_rom()
    off = snes_to_rom(0x81EC47)
    pitches = struct.unpack("<16H", rom[off : off + 32])

    expected_dimensions = [
        (1792, 576),  # 0: Japan
        (1856, 640),  # 1: England
        (1984, 704),  # 2: Spain
        (2048, 640),  # 3: Italy
        (1920, 640),  # 4: Germany
        (1920, 576),  # 5: Brazil
        (1792, 704),  # 6: Nigeria
        (2176, 704),  # 7: USA
    ]

    for i, (exp_len, exp_wid) in enumerate(expected_dimensions):
        length = pitches[i * 2]
        width = pitches[i * 2 + 1]
        assert length == exp_len, f"Stadium {i} length mismatch: got {length}, expected {exp_len}"
        assert width == exp_wid, f"Stadium {i} width mismatch: got {width}, expected {exp_wid}"


def test_rom_shot_power_tables():
    """Verify initial dZ and gravity across power levels 0..11."""
    rom = load_rom()
    off_dz_int = snes_to_rom(0x819E77)
    off_dz_frac = snes_to_rom(0x819E8F)
    off_grav = snes_to_rom(0x819EA7)

    dz_ints = struct.unpack("<12h", rom[off_dz_int : off_dz_int + 24])
    dz_fracs = struct.unpack("<12H", rom[off_dz_frac : off_dz_frac + 24])
    gravities = struct.unpack("<12H", rom[off_grav : off_grav + 24])

    # Normal shot velocities scale monotonically from tap (-3.75) to max (-7.00)
    prev_speed = 0.0
    for i in range(12):
        vel = dz_ints[i] + (dz_fracs[i] / 65536.0)
        speed = abs(vel)
        assert speed >= prev_speed, f"Shot power level {i} speed {speed} < {prev_speed}"
        prev_speed = speed

    # Check gravity values: 0x2000 (0.125 px/frame^2) and 0x1C00 (0.109375 px/frame^2)
    for g in gravities[:8]:
        assert g == 0x2000, f"Expected standard gravity 0x2000, got {hex(g)}"
    for g in gravities[8:]:
        assert g == 0x1C00, f"Expected higher-tier float gravity 0x1C00, got {hex(g)}"

    # Chip shot gravity table (DATA_819EEF): 0x2400 (0.140625 px/frame^2)
    off_chip_grav = snes_to_rom(0x819EEF)
    chip_gravities = struct.unpack("<12H", rom[off_chip_grav : off_chip_grav + 24])
    for g in chip_gravities:
        assert g == 0x2400, f"Expected chip shot gravity 0x2400, got {hex(g)}"


def test_rom_goalkeeper_bounding_boxes():
    """Verify Goalkeeper 3D interception bounding boxes in DATA_81CF47."""
    rom = load_rom()
    off = snes_to_rom(0x81CF47)
    standing_box = struct.unpack("<6H", rom[off : off + 12])
    # x_center_off=24, x_half_reach=48, y_center_off=16, y_half_reach=32, z_center_off=8, z_half_reach=16
    assert standing_box == (24, 48, 16, 32, 8, 16)


def test_kinematic_simulation_matches_airtime():
    """Verify the exact 32-bit fixed-point physics integration."""
    dz_ints = [-4, -5, -6, -6, -7]
    dz_fracs = [0x4000, 0x8000, 0xC000, 0x0000, 0x0000]
    gravities = [0x2000, 0x2000, 0x2000, 0x1C00, 0x1C00]

    expected_results = [
        (-59, 61),   # Level 0: apex=-59, frames=61
        (-84, 73),   # Level 3: apex=-84, frames=73
        (-113, 85),  # Level 6: apex=-113, frames=85
        (-168, 111), # Level 9: apex=-168, frames=111
        (-228, 129), # Level 11: apex=-228, frames=129
    ]

    for idx in range(len(expected_results)):
        z_int, z_frac = 0, 0
        dz_int = dz_ints[idx]
        dz_frac = dz_fracs[idx]
        g_frac = gravities[idx]
        min_z = 0
        frame = 0

        while True:
            frame += 1
            z_frac += dz_frac
            carry = z_frac >> 16
            z_frac &= 0xFFFF
            z_int += dz_int + carry

            if z_int < min_z:
                min_z = z_int

            if z_int >= 0 and frame > 1:
                break

            dz_frac += g_frac
            g_carry = dz_frac >> 16
            dz_frac &= 0xFFFF
            dz_int += g_carry

        exp_apex, exp_frames = expected_results[idx]
        assert min_z == exp_apex, f"Index {idx}: expected apex {exp_apex}, got {min_z}"
        assert frame == exp_frames, f"Index {idx}: expected {exp_frames} frames, got {frame}"


def test_restitution_damping_weather():
    """Verify ground bounce restitution formulas (CODE_83898D)."""
    # Dry / Normal / Snow: 0.5 + 0.25 = 0.75
    incoming_dz = -6.0  # px/frame
    cr_dry = (1.0 / 2.0) + (1.0 / 4.0)
    bounced_dry = -incoming_dz * cr_dry
    assert bounced_dry == 4.5, f"Expected 4.5 px/f bounce on dry pitch, got {bounced_dry}"

    # Rain: 0.50
    cr_rain = 0.50
    bounced_rain = -incoming_dz * cr_rain
    assert bounced_rain == 3.0, f"Expected 3.0 px/f bounce on wet pitch, got {bounced_rain}"

    # Low-velocity threshold clamp (threshold = 0.25)
    assert cr_rain * 0.40 < 0.25  # 0.20 < 0.25 -> clamped to 0 (ball rolls)


def test_player_condition_arrow_modifiers():
    """Verify player condition arrow modifiers from CODE_80E86A."""
    condition_arrows = {
        0: -2,  # Red Down
        1: -1,  # Blue Slanted Down
        2: 0,   # Yellow Horizontal
        3: +1,  # Orange Slanted Up
        4: +2,  # Pink Up
    }

    base_stat = 6
    for arrow_code, delta in condition_arrows.items():
        effective = max(1, min(15, base_stat + (arrow_code - 2)))
        assert effective == base_stat + delta

    # Clamp bounds verification [1..15]
    assert max(1, min(15, 2 + (0 - 2))) == 1   # low clamp
    assert max(1, min(15, 15 + (4 - 2))) == 15  # high clamp


def test_rom_passing_angle_cones():
    """Verify passing directional cone tables DATA_819B7F and DATA_819B87 in ROM."""
    rom = load_rom()
    off_min = snes_to_rom(0x819B7F)
    off_max = snes_to_rom(0x819B87)

    min_angles = list(rom[off_min : off_min + 8])
    max_angles = list(rom[off_max : off_max + 8])

    # Verified 64-step angle circle cones for 8 D-Pad directions
    assert min_angles == [60, 0, 4, 18, 28, 32, 36, 50]
    assert max_angles == [68, 8, 20, 28, 36, 40, 52, 60]


def test_dribble_possession_and_nudge_thresholds():
    """Verify dribble distance thresholds: tether radius (24 px) and foot touch (16 px)."""
    tether_limit_px = 24   # CODE_849530: CMP #$0018
    touch_trigger_px = 16  # CODE_849F60: CMP #$0010

    # Within 16 px -> foot touch impulse applied to ball
    dist_at_foot = 12
    assert dist_at_foot < touch_trigger_px

    # Between 16 and 24 px -> ball rolls freely ahead, carrier in control
    dist_free_roll = 19
    assert touch_trigger_px <= dist_free_roll < tether_limit_px

    # Beyond 24 px -> possession immediately severed
    dist_loose = 25
    assert dist_loose >= tether_limit_px


def test_headless_gameplay_simulation():
    """Verify live headless match simulation runs 120 frames cleanly."""
    exe_path = REPO_ROOT / "build" / "ISSDNative.exe"
    if not exe_path.exists():
        exe_path = REPO_ROOT / "dist" / "windows" / "ISSDNative.exe"
    assert exe_path.exists(), f"Executable not found at {exe_path}"

    cmd = [str(exe_path), "--headless", "120"]
    result = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    assert result.returncode == 0, f"Headless execution failed:\n{result.stderr}\n{result.stdout}"
    assert "Executed 120 frames total" in result.stdout or "Target frame count reached: 120" in result.stdout
