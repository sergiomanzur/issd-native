# Stadium construction investigation

This is an investigation record, not an acceptance certificate for independent
stadium geometry or artwork. The implementation contract is in
`STADIUM_PROFILE_CONTRACT.md`.

The first instrumented native build captured all eight original stadiums from
a fresh Exhibition menu, without changing guest memory. At frame 5000 each
capture had game mode 8, logical stadium ID equal to its selected original ID,
and the following constructor dimensions:

| Original layout | Length | Width |
|---|---:|---:|
| 0 | 1792 | 576 |
| 1 | 1856 | 640 |
| 2 | 1984 | 704 |
| 3 | 2048 | 640 |
| 4 | 1920 | 640 |
| 5 | 1920 | 576 |
| 6 | 1792 | 704 |
| 7 | 2176 | 704 |

The private evidence is under
`build/independent-stadium-validation/original-contract/`. Its `evidence.json`
records cartridge, executable and instrumentation source hashes and retains
`complete: false`. WRAM dumps, cartridge data and extracted art are not shipped
with example packs.

The first capture demonstrates why a function-entry trace is insufficient:
native block callbacks observed constructor entry/return, decompression entry,
palette entry/return, streamer entry and subsequent PPU rendering, but omitted
some interior DMA completion instruction boundaries. The interpreter has
instruction callbacks; the native generator now needs selected interior opcode
hooks as well. These hooks preserve instruction register, flag and cycle
semantics and invoke the existing host callback only when it is installed.

Relevant candidate boundaries are `80B909`, `80B90D`, `808DB8`, `8B8CEC`,
`8B8DC1`, `8B85E3` and `8B86E9`. A candidate address is not proof that a whole
scene's transfers have completed. Pending uploads and transfer generations must
be classified before producing the acceptance certificate. A replay-table read
during scene setup also does not establish that a genuine goal replay ran.

The certificate validator rejects missing original layouts, missing replay
routes, mixed upload generations, reversed upload/stream order, modified guest
memory and unverified pitch/net scenery envelopes. Its unit fixtures test the
validator and are not native gameplay evidence.

Next evidence requirements are completion-order classification from the precise
native hooks, genuine replay routes, complete scenery partitions and indirect
geometry-consumer coverage. Profile validation and registry construction are
implemented separately; profiles are not yet wired into gameplay.

The second build includes the audited interior DMA callbacks. Its eight fresh
captures are stored privately in `original-contract-precise`. In layout 0,
background streaming occurs in mode 4 before the pitch constructor in mode 6;
the constructor tick is 4242844. Consequently the validator checks dependencies:
palette and decompression completion precede DMA completion, DMA precedes the
certified streamer and presentation, and pitch construction precedes gameplay
presentation. Palette/decompression are not assigned an invented mutual order.
Candidate callbacks still do not certify scene-wide completion or a replay.

A further Ghidra export corrected the palette boundary: `8B8DC1` is entry
to the palette-copy helper; `8B8DC0` is the enclosing loader's return. Older
raw events named `palette_return` at `8B8DC1` cannot certify completion.
The current instrumentation uses `85A50A` for selection generation and records
the deferred-descriptor count (`0130`), queued DMA end (`0048`), defer state,
submode and scores. `808DB8` follows DMA queue draining and reset to `3200`;
it does not alone prove the deferred decompression queue is empty. Both the
queue state and source generation require classification. The private Ghidra
listing is `stadium-transfer-order-evidence.txt` and has `EXPORT_COMPLETE`.
