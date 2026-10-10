#include "issd_stadium_scene.h"
#include "issd_stadium_rom.h"
#include "issd_stadium_geometry.h"
#include "issd_stadium_assets.h"
#include "issd_stadium_allocations.h"
#include "snes/cart.h"
#include "snes/ppu.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static IssdStadiumRom scene;
static IssdStadiumGeometry geometry;
static unsigned selected = 32, generation;
static bool maps_pending;
static bool saved_art_known;
static uint8_t saved_art[16];
static void art_mix(uint64_t state[2], const void *source, size_t size) {
    const uint8_t *bytes = source;
    for (size_t i = 0; i < size; ++i) {
        state[0] = (state[0] ^ bytes[i]) * UINT64_C(1099511628211);
        state[1] = (state[1] ^ bytes[i]) * UINT64_C(14029467366897019727);
    }
}
static unsigned word(const uint8_t *ram, unsigned address) {
    return ram[address] | (unsigned)ram[address+1] << 8;
}
void issd_stadium_scene_art_identity(const uint8_t ram[0x20000], uint8_t identity[16]) {
    memset(identity,0,16);
    const IssdStadiumAssets *assets = issd_stadium_assets(word(ram,0x1fa2));
    if (!assets) return;
    /* Cosmetic change detector only; geometry compatibility has its own SHA256.
     * Hash resource bytes, never paths, pointers or structure padding. */
    uint64_t state[2] = {UINT64_C(14695981039346656037),UINT64_C(7809847782465536322)};
    art_mix(state,assets->geometry.metatiles,sizeof assets->geometry.metatiles);
    art_mix(state,assets->geometry.world_maps,sizeof assets->geometry.world_maps);
    art_mix(state,assets->palette,assets->palette_size);
    for (size_t i=0;i<assets->write_count;++i) {
        uint8_t destination[2] = {(uint8_t)assets->writes[i].word_destination,
                                 (uint8_t)(assets->writes[i].word_destination>>8)};
        art_mix(state,destination,2);
        art_mix(state,assets->writes[i].data,assets->writes[i].size);
    }
    for (unsigned i=0;i<16;++i) identity[i]=(uint8_t)(state[i/8]>>(8*(i%8)));
}
void issd_stadium_scene_saved_art(const uint8_t identity[16], bool known) {
    saved_art_known=known;
    if (known) memcpy(saved_art,identity,16);
}
void issd_stadium_scene_refresh_art(struct Ppu *ppu, uint8_t ram[0x20000]) {
    if (!saved_art_known || selected==32 || !ppu) return;
    uint8_t identity[16];issd_stadium_scene_art_identity(ram,identity);
    if (!memcmp(identity,saved_art,16)) { saved_art_known=false;return; }
    const IssdStadiumAssets *assets=issd_stadium_assets(selected);
    if (word(ram,0x1fa2)!=selected || word(ram,0x86)==8 ||
        (ppu->bgmode&7)!=1 || (ppu->bgTileAdr&0xff)!=0x22 ||
        word(ram,0x100) || word(ram,0x1f00) || word(ram,0x130)) return;
    /* Original palette programs own fades and rotations. Wait for all four
     * controllers to finish, then refresh their source/copy once and request
     * the original CGRAM upload instead of writing hardware palette state. */
    for (unsigned address=0x11a0;address<=0x11d0;address+=16)
        if (word(ram,address)) return;
    const IssdStadiumProfile *profile=issd_stadium_profile(selected);
    uint8_t *native=malloc(65536),palette[512];
    if (!native || !profile || !issd_stadium_read_native_resources(scene.data,scene.size,
                                                profile->base_layout,native,palette)) {
        free(native);return;
    }
    for (unsigned layer=0;layer<2;++layer) {
        memcpy(ram+0x18000+layer*0x2000,geometry.metatiles[layer],8192);
        memcpy(ram+0x1d000+layer*0x1000,geometry.world_maps[layer],4096);
    }
    /* Restore every owned static span first, including uploads that were
     * deleted from the pack. HUD, OBJ and animated allocations are excluded. */
    for (unsigned slot=0;slot<5;++slot) {
        size_t start=issd_stadium_slots[profile->base_layout][slot][0]*2;
        size_t count=issd_stadium_slots[profile->base_layout][slot][1]*32;
        if (count) memcpy((uint8_t*)ppu->vram+start,native+start,count);
    }
    free(native);
    size_t palette_size=(profile->base_layout==2 || profile->base_layout==6)?64:96;
    if (assets) {
        for (size_t i=0;i<assets->write_count;++i)
            memcpy((uint8_t*)ppu->vram+assets->writes[i].word_destination*2,
                   assets->writes[i].data,assets->writes[i].size);
        if (assets->palette_size) memcpy(palette+0x40,assets->palette,assets->palette_size);
    }
    memcpy(ram+0x2c40,palette+0x40,palette_size);
    memcpy(ram+0x2e40,palette+0x40,palette_size);
    ram[0x41]|=0x80;
    saved_art_known=false;
}
static bool compile_geometry(unsigned id,const IssdStadiumProfile *profile,
                             const uint8_t *canonical,size_t size) {
    const IssdStadiumAssets *assets = issd_stadium_assets(id);
    if (assets) {
        /* Final authored maps already passed allocation validation. An unused
         * template transform may overflow; it is not a dependency of these maps. */
        geometry = assets->geometry;
        return true;
    }
    IssdStadiumGeometry original;
    return issd_stadium_read_template(canonical,size,profile->base_layout,&original) &&
           issd_stadium_geometry_compile(&original,profile,&geometry);
}
void issd_stadium_scene_transfer(struct Ppu *ppu, uint8_t ram[0x20000], uint32_t pc) {
    pc &= 0x7fffff;
    if (pc != 0x0b8dc0 && pc != 0x0b8cc4 && pc != 0x00b909 && pc != 0x00b518)
        return;
    unsigned mode = word(ram,0x70);
    if (!ppu || word(ram,0x86) == 8 ||
        (mode != 4 && mode != 6 && mode != 8 && mode != 0x13)) return;
    const IssdStadiumAssets *assets = issd_stadium_assets(word(ram,0x1fa2));
    if (!assets) return;
    if (pc == 0x0b8dc0) {
        /* Join the original palette source before its normal copy/fade path.
         * CGRAM and the working fade copy retain their original lifecycle. */
        memcpy(ram+0x2c40,assets->palette,assets->palette_size);
        return;
    }
    if ((ppu->bgmode&7) != 1 || (ppu->bgTileAdr&0xff) != 0x22) return;
    if (word(ram,0x100) || word(ram,0x1f00) || word(ram,0x130)) return;
    /* Apply only measured native character spans after an original completed
     * transfer. Later stock reloads pass this same seam, so they cannot leave
     * the final scene containing stale template characters. */
    for (size_t i = 0; i < assets->write_count; ++i) {
        const IssdStadiumTileWrite *write = &assets->writes[i];
        memcpy((uint8_t *)ppu->vram+write->word_destination*2,write->data,write->size);
    }
}
void issd_stadium_scene_reset(struct Cart *cart) {
    if (cart) cart_clearRomView(cart);
    issd_stadium_rom_clear(&scene);
    selected = 32;
    generation = 0;
    maps_pending = false;
}
bool issd_stadium_scene_restore(struct Cart *cart, const uint8_t ram[0x20000],
                                const uint8_t *canonical, size_t size) {
    unsigned id = word(ram,0x1fa2), mode = word(ram,0x70);
    const IssdStadiumProfile *profile = issd_stadium_profile(id);
    issd_stadium_scene_reset(cart);
    if (!profile || word(ram,0x86) == 8 ||
        (mode != 4 && mode != 6 && mode != 8 && mode != 0x13)) return true;
    if (!compile_geometry(id,profile,canonical,size) ||
        !issd_stadium_rom_build(&scene,canonical,size,profile) ||
        !cart_setRomView(cart,scene.data,scene.size)) {
        issd_stadium_scene_reset(cart);
        return false;
    }
    selected = id;
    generation = issd_stadium_generation();
    return true;
}
bool issd_stadium_scene_opcode(struct Cart *cart, uint8_t ram[0x20000],
                               const uint8_t *canonical, size_t size, uint32_t pc) {
    pc &= 0x7fffff;
    if (issd_stadium_trace_enabled() &&
        (pc == 0x05a50a || pc == 0x03b082 || pc == 0x03b165 || pc == 0x24e0b3))
        fprintf(stderr, "[StadiumScene] pc=%06x mode=%x id=%u profiles=%u selected=%u view=%u\n",
                pc, word(ram,0x70),word(ram,0x1fa2),issd_stadium_has_profiles(),
                selected,cart && cart->romView ? 1u : 0u);
    if (pc == 0x03b165) { issd_stadium_scene_reset(cart); return true; }
    if (pc == 0x05a50a) {
        const IssdStadiumProfile *profile = issd_stadium_profile(word(ram,0x1fa2));
        issd_stadium_scene_reset(cart);
        if (profile) {
            ram[0x86] = (uint8_t)profile->base_layout;
            ram[0x87] = 0;
        }
        return true;
    }
    unsigned mode = word(ram,0x70);
    bool match_stream = (pc == 0x0b85e3 || pc == 0x0b86e9) &&
        (mode == 4 || mode == 6 || mode == 8 || mode == 0x13) && word(ram,0x86) != 8;
    bool match_boundary = match_stream || (pc == 0x0b8000 && mode == 4 && word(ram,0x86) != 8);
    if (match_boundary && selected != 32 &&
        (selected != word(ram,0x1fa2) || generation != issd_stadium_generation()))
        issd_stadium_scene_reset(cart);
    if (pc == 0x0b8ceb && selected != 32) maps_pending = true;
    if (match_boundary && selected == 32) {
        unsigned id = word(ram, 0x1fa2);
        const IssdStadiumProfile *profile = issd_stadium_profile(id);
        issd_stadium_scene_reset(cart);
        if (!profile) return true;
        if (!compile_geometry(id,profile,canonical,size) ||
            !issd_stadium_rom_build(&scene, canonical, size, profile) ||
            !cart_setRomView(cart, scene.data, scene.size)) {
            issd_stadium_scene_reset(cart);
            return false;
        }
        selected = id;
        generation = issd_stadium_generation();
        maps_pending = true;
        ram[0x86] = (uint8_t)profile->base_layout;
        ram[0x87] = 0;
    }
    /* The original loader has populated map RAM before the first world
     * streamer. Install once, preserving subsequent animated metatiles. */
    if (match_stream && maps_pending && !word(ram,0x100) && !word(ram,0x1f00) &&
        selected == word(ram, 0x1fa2) && generation == issd_stadium_generation()) {
        for (unsigned layer = 0; layer < 2; ++layer) {
            memcpy(ram + 0x18000 + layer*0x2000, geometry.metatiles[layer], 8192);
            memcpy(ram + 0x1d000 + layer*0x1000, geometry.world_maps[layer], 4096);
        }
        maps_pending = false;
    }
    return true;
}
