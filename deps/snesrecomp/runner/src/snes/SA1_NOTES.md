# Nintendo SA-1 support

The SA-1 is a cartridge coprocessor containing a second 65816-class CPU and
memory/peripheral hardware. It is not modeled as a game-specific accelerator:
the cartridge's SA-1 program executes instruction by instruction through the
shared MIT-licensed `interp816` CPU core.

## Implemented hardware

- Header detection for SA-1 cartridge type nibble `$3`
- 2 KiB internal RAM and battery-backed BW-RAM
- Super MMC four-segment ROM selection
- S-CPU and SA-1 BW-RAM windows, including 2-bpp/4-bpp bitmap access
- CPU/SA-1 message, IRQ/NMI, and interrupt-vector override registers
- Horizontal/vertical timer
- Normal DMA and type-1/type-2 character conversion DMA
- Multiply, divide, cumulative multiply, and arithmetic overflow
- Variable-length bit reader
- Save/load state and instruction/clock observability

The core advances at one SA-1 CPU cycle per two SNES master clocks. Main-CPU
memory accesses and idle frame scheduling both synchronize the coprocessor,
so polling loops, WAI wakeups, DMA results, and interrupt delivery observe a
single monotonic hardware timeline.

## Validation

`tests/sa1/` contains synthetic cartridge tests for header detection, reset
hold/release, instruction execution, IRAM/BW-RAM access, Super MMC banking,
arithmetic, S-CPU interrupt signaling, and clock synchronization. Run the
complete regression set with:

```sh
bash tests/run_c_tests.sh
```

The canonical US *Super Mario RPG* ROM is the first full-title validation
target. Its 18,000-frame unattended attract-loop qualification retires more
than 946 million SA-1 instructions while game state, video, and audio remain
active. A 600-frame Snes9x comparison matches the same opening scene within a
six-frame startup phase offset and 0.73 mean RGB error. Its ROM data and
generated source are not part of this repository.

## Provenance

The peripheral and mapping behavior is adapted from the ISC-licensed ares
SA-1 implementation. See `THIRD_PARTY_ATTRIBUTION.md`. No GPL or
noncommercial emulator source is incorporated.
