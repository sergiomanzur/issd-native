"""Canonical names for generated v2 C symbols."""


def default_func_name(bank: int, start: int) -> str:
    """Return the synthetic base name for an unnamed SNES function."""
    return f"bank_{bank:02X}_{start:04X}"


def variant_suffix(m: int, x: int) -> str:
    """Return the universal M/X-width suffix for a generated function."""
    return f"_M{m & 1}X{x & 1}"
