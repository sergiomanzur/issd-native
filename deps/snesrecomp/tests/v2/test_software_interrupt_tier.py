"""BRK and COP transfer at their exact instruction instead of stubbing."""

from _helpers import make_lorom_bank0

from v2.emit_function import emit_function


def test_brk_and_cop_transfer_to_lle_at_the_instruction_site():
    for opcode, kind in ((0x00, "BRK"), (0x02, "COP")):
        rom = make_lorom_bank0({0x8000: bytes([opcode, 0x00])})
        source = emit_function(
            rom, bank=0, start=0x8000, entry_m=1, entry_x=1,
            end=0x8002)

        assert f"/* {kind}: software interrupt */" not in source
        assert f"/* {kind}: execute exact software interrupt" in source
        assert (
            "return interp_tier_dispatch_tail(cpu, 0x008000u, "
            "0x008000u, _entry_s, _hrv);"
        ) in source
