# Stadium profile v1 implementation contract

This contract records the bounded implementation established by the 2026-10-09
investigation. Measured implementation coverage and its limits are recorded in
[Independent stadium acceptance](INDEPENDENT_STADIUM_ACCEPTANCE.md).

## Coordinates and geometry

Engine coordinates are integer pixels in a sheared projection, not display yards.
For world coordinates `(u,v)`, screen coordinates are
`x=u+v-camera_x`, `y=v-camera_y`. The playable origin is fixed at `(0,0)`.
The opposite boundary is `(length_units,width_units)`. Object fractions remain
separate. Version 1 retains symmetric centered goals, the original net depths
and crossbar height, and template scenery collision policies.

Public JSON entry:

```json
{
  "stadium_id": 8,
  "name": "CUSTOM",
  "display_name": "CUSTOM GROUND",
  "pitch_length": 114,
  "pitch_width": 74,
  "stadium_profile": {
    "version": 1,
    "base_layout": 0,
    "geometry": {
      "length_units": 1728,
      "width_units": 576,
      "camera": {
        "min_y": 96,
        "max_y": 672,
        "left_shear_anchor": 256,
        "max_x_cap": 2368,
        "right_shear_anchor": 2000
      }
    },
    "artwork": "custom_ground/stadium.json"
  }
}
```

`pitch_length`/`pitch_width` retain their existing display-only meaning. No
conversion between these yard labels and engine units is asserted.
Omitted camera values inherit the base template. A profile without artwork can
use compiled template art with regenerated pitch markings; it must still pass
geometry acceptance. An artwork path is resolved relative to the pack file.

The v1 authoring policy requires lengths divisible by 32, from 1536 up to the
selected base's length, and widths divisible by 32, from 512 up to its width.
These are conservative product restrictions for fixed-origin, shrink-only
profiles, not an assertion that every value is already proven safe. Validate
derived regions and projected geometry against the template map/scenery envelope.
If a template/value fails that proof, reject it explicitly; do not accept it and
silently use stock geometry. The first two acceptance fixtures use base 0 with
lengths 1728 and 1664, and width 576, to isolate the longitudinal change.

Goals have inner transverse bounds `width/2 ± 48`, outer bounds `width/2 ± 56`,
and post thickness 8. Opposing longitudinal positions derive from the two pitch
ends. Markings preserve the original rules and move with the pitch dimensions;
arbitrary translated pitch origins and independently resized goals/areas are
not v1 controls. The editor shows these derived positions and allows dragging
the pitch ends/sidelines through the same length/width fields.

Camera inputs are unsigned 16-bit engine coordinates. Require ordered Y bounds,
valid projected X intervals over the entire Y range, and complete map coverage
of the view and goal frames. Template envelope checks, rather than merely the
16-bit storage limit, define accepted values. Initial/goal framing positions are
derived from the compiled pitch/map and must be consistent during replay.

## Original values and consumers

| Base | Length | Width | min Y | max Y | left anchor | X cap | right anchor |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 1792 | 576 | 96 | 672 | 256 | 2368 | 2000 |
| 1 | 1856 | 640 | 96 | 704 | 192 | 2464 | 2000 |
| 2 | 1984 | 704 | 64 | 736 | 192 | 2688 | 2304 |
| 3 | 2048 | 640 | 64 | 736 | 192 | 2688 | 2304 |
| 4 | 1920 | 640 | 96 | 704 | 192 | 2560 | 2304 |
| 5 | 1920 | 576 | 128 | 640 | 192 | 2464 | 2000 |
| 6 | 1792 | 704 | 64 | 736 | 192 | 2464 | 2000 |
| 7 | 2176 | 704 | 64 | 704 | 192 | 2880 | 2368 |

`A4E0B3` reads pitch table `81EC47` and derives gameplay fields. For `L,W`:

```text
12A2=L; 12A4=W; 12F2=L/2; 12D6=12D8=W/2; 12D0=W/2-128
12C6=W/2-128; 12C8=W/2-192; 12CA=W/2+128; 12CC=W/2+192
12E4=W/2-48; 12E6=W/2+48; 12E0=W+24
12E8=-56; 12EA=L+56; 12E2=L+128
12C2=L-64; 12B4=L-384; 12B6=12C0=L-768; 12BE=L-736
12C4=L/2+160; 12B8=L/2-128; 12BA=L/2-256; 12BC=L/2+384
12CE=W-32
```

`8386BF` initializes goal/net fields `1C80..1C8C`. Camera construction is in
`8B8000` and `8B8C90`; `8B88A5..8B88CC` reread tables. Replay paths
`A4D6B2/A4D6C5` directly reread pitch and framing tables. Constructor completion
PCs exist at `8BDB65`, `98F205`, and `A4D7CE`, not inside the single-block AOT
constructor. Updating derived RAM alone therefore cannot implement profiles.

Baked immediate comparisons need actual dynamic operands: `838B27` uses
`0708` for a post threshold, `838E1A` uses `0380` for field half, and `838EB5`
contains a fixed penalty predicate. `8B82AA` and `838BF8` have camera thresholds
that must use projected framing semantics, not blindly `L/2`. Audit the eight
scenery callbacks selected by `83A07C` and indirect geometry references before
enabling a profile. The ignored investigation report contains 398 explicit
geometry operand references; an explicit reference inventory is not proof that
all indirect consumers have been covered.

## Native artwork and allocation

The asset manifest has `version:1`, `base_layout`,
`allocation_profile:"original-scenery-v1"`, `layers`, `tiles`, `palette`, and
optional `hd`. Paths inside it are relative to the manifest directory.

Each layer has `layer` (0/1), `pages_x`, `pages_y`, `metatiles` and `world_map`.
Both layers use the same page dimensions and stride `pages_x*64`. Version 1
retains the template's page dimensions; do not treat a 4096-byte map buffer as
permission to change the world's extent without matching all original stride
consumers. The compiler regenerates pitch geometry within that envelope.

Each layer has exactly 8192 bytes of little-endian metatile definition words
(256 32x32 metatiles, each sixteen 8x8 tile words), and a 4096-byte padded page
map. Original WRAM destinations are `18000/1A000` and `1D000/1E000`. Validate
references and map padding; no writes outside these measured layer buffers.

Native tile resources contain `slot`, `offset_tiles`, `tile_count`, and `file`.
Files contain exactly `tile_count*32` planar 4bpp bytes. Slots are original
descriptor indices below; arbitrary VRAM destinations are forbidden.
Destinations in the table are VRAM **word** addresses, sixteen words per tile.

| Base | Ordered slot destination : tile count |
|---|---|
| 0 | 4000:95, 45F0:11, 46A0:22 |
| 1 | 4000:31, 4200:32, 4400:27, 4600:32 |
| 2 | 4000:48, 4300:27 |
| 3 | 41D0:19, 4000:45, 4300:32, 4500:22 |
| 4 | 4000:31, 4200:27, 4400:32, 4600:22, 4750:23 |
| 5 | 4000:68, 4750:23, 4500:48, 48C0:3 |
| 6 | 4100:64, 4000:46, 4500:25, 48C0:3, 4750:23 |
| 7 | 4000:16, 4600:22 |

These are owned scenery spans derived from reload descriptors `81AC2F` and
decompressed output counts, not free allocations. Preserve descriptor order
when building template data, particularly overlapping spans in base 6. Export
canonical final tile writes and reject duplicate writes in authored overrides.
Assert the expected gameplay character base (VRAM word 2000) before application.
Leave HUD, OBJ, common graphics, and other native allocations intact.

`palette` is a relative path to little-endian BGR555 colors, exactly 64 bytes for
bases 2/6 and 96 bytes otherwise. Palette resources start at CGRAM entry 20 hex;
only 32/48 colors belong to the profile. Preserve transparency and unrelated
banks. BGR555 bit 15 must be zero; index zero of each 16-color bank retains
native transparency. Update original palette source/mirror construction (`7E2C40`), including
fade copies, rather than forcing final CGRAM every frame. Weather variants for
bases 2/6 can upload an extra stock bank; it is not a profile allocation.

`hd` is a list of `{ "key": "16hexcharacters", "file": "relative.bmp" }`.
The editor derives keys from compiled native tiles/palettes. Local HD lookup is
keyed by logical stadium, profile generation and tile key and falls back to enabled global HD, then
native art. Existing square, 32-bit BMP, edge-multiple-of-eight limits apply.
Native artwork remains visible at 1X without any HD setting.

Editor-generated manifests also retain `generated_geometry`, a `geometry`
length/width tag, and upload metadata `palette_bank`, optional `placement`
(`layer`, `x`, `y` in native pixels), `image_width`, `image_height`, and
`hd_keys`. The tag rejects stale generated maps. Recompilation reapplies stored
placements with copy-on-write metatile deduplication; overflow rejects the new
directory. The editor rejects moving/resizing an existing placement and sharing
its authored palette bank with another upload. Runtime resources remain the
validated final binary maps, tile writes, palette and local HD list.

The initial art compiler allows authoring within these measured native budgets.
It reports quantization loss and rejects tile/palette overflow. It uses the same
map/marking compiler for preview and exported data. It does not distribute
extracted cartridge art in public example packs.

## Runtime and validation requirements

An immutable effective profile registry follows mod stack precedence. A private
selected-scene ROM overlay must be visible through CPU, interpreter, DMA and fast
ROM-pointer paths, while canonical effective ROM/save identity remains stable.
Special introduction layout 8 bypasses profiles. Both AOT and interpreter routes
must observe dynamic immediate operands and table reads; changing ROM bytes alone
does not change baked generated constants. Generator changes require normal
regeneration, never hand editing generated C.

Original graphics loading can defer uploads through `80B527`. Applying profile
art at constructor entry is insufficient. Application must use a proven
transfer-completion/construction boundary and survive subsequent original DMA,
replay reload and snapshot restoration. Physics changes occur only at proven
construction/predicate boundaries; presentation must not rewrite them per frame.
Transfer-completion candidates are `80B909/80B90D` for synchronous uploads and
`808DB8` after the queued batch resets its pending/staging pointers. These are
per-transfer/batch boundaries, not proof of a completed scene. Require a pending
profile scene generation and verify that all required uploads are complete
before publishing it; arbitrary NMI completion must not apply a stadium.

Canonical gameplay profile identity enters save compatibility, including profiles
implemented through host hooks. No-profile saves preserve their existing context.
Pure cosmetic HD changes do not invalidate gameplay saves. Real restored saves,
goals, penalties, throw-ins, corners, goal kicks, framing and scenery interactions
are required evidence. See the acceptance report for the distinction between
controlled original-code boundary fixtures and uninterrupted natural matches.
