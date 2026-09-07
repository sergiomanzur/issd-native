"""Automatic LLE tier-down for interrupt-owned memory polling loops."""

from v2.emit_function import emit_function


def _rom(code: bytes) -> bytes:
    return code + bytes(0x8000 - len(code))


def test_pure_memory_self_poll_unwinds_to_lle_interpreter():
    # PHP; SEP #$20; LDA $05B8; CMP $05B8; BEQ -5; PLP; RTS
    code = bytes.fromhex("08 E2 20 AD B8 05 CD B8 05 F0 FB 28 60")
    src = emit_function(_rom(code), bank=0, start=0x8000,
                        entry_m=0, entry_x=0, end=0x800D)

    assert "if (interp_bridge_in_lle_scheduler())" in src
    assert "interp_bridge_lle_yield_unwind(cpu, 0x008000u)" in src


def test_counter_loop_stays_compiled():
    # LDX #$03; DEX; BNE -3; RTS -- a finite CPU loop, not an interrupt poll.
    code = bytes.fromhex("A2 03 CA D0 FD 60")
    src = emit_function(_rom(code), bank=0, start=0x8000,
                        entry_m=1, entry_x=1, end=0x8006)

    assert "interp_bridge_in_lle_scheduler()" not in src
    assert "goto L_8002_M1X1" in src
    assert "cpu->X = (uint16)((cpu->X) + (-1))" in src


def test_apu_port_echo_poll_advances_device_and_returns_through_rts():
    # STA $2140; CMP $2140; BNE $8000; RTS. The SPC must advance between
    # iterations before the port can echo the byte written by the CPU.
    code = bytes.fromhex("8D 40 21 CD 40 21 D0 F8 60")
    src = emit_function(_rom(code), bank=0, start=0x8000,
                        entry_m=1, entry_x=1, end=0x8009)

    assert "RtlApuWriteWaitEcho(cpu, 0x2140u, cpu_read_a8(cpu), 0)" in src
    assert "goto L_8008_M1X1; /* device echo poll completed */" in src
    assert "interp_tier_dispatch_balanced(cpu, 0x008000u, 0x008000u" not in src


def test_apu_echo_poll_inside_larger_function_is_replaced_at_its_block_only():
    # NOP; STA $2140; CMP $2140; BNE $8001; RTS. A root that begins before
    # the polling block must execute its prefix and only replace the poll.
    code = bytes.fromhex("EA 8D 40 21 CD 40 21 D0 F8 60")
    src = emit_function(_rom(code), bank=0, start=0x8000,
                        entry_m=1, entry_x=1, end=0x800A)

    assert "L_8000_M1X1:" in src
    assert "goto L_8001_M1X1; /* implicit fall-through */" in src
    assert "RtlApuWriteWaitEcho(cpu, 0x2140u, cpu_read_a8(cpu), 0)" in src
    assert "goto L_8009_M1X1; /* device echo poll completed */" in src
