#include "issd_stadium.h"
#include "issd_mod.h"
#include "sha256.h"
#include "issd_asset_path.h"
#include "issd_stadium_assets.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static IssdStadiumProfile s_profiles[ISSD_MAX_STADIUMS];
static bool s_present[ISSD_MAX_STADIUMS];
static unsigned s_generation;
static IssdStadiumAssets *s_assets[ISSD_MAX_STADIUMS];

void issd_stadium_clear(void) {
    bool changed = issd_stadium_has_profiles();
    memset(s_profiles, 0, sizeof s_profiles);
    memset(s_present, 0, sizeof s_present);
    for (unsigned id = 0; id < ISSD_MAX_STADIUMS; ++id) {
        issd_stadium_assets_free(s_assets[id]); s_assets[id] = NULL;
    }
    if (changed) ++s_generation;
}

bool issd_stadium_has_profiles(void) {
    for (unsigned i = 0; i < ISSD_MAX_STADIUMS; ++i)
        if (s_present[i]) return true;
    return false;
}

const IssdStadiumProfile *issd_stadium_profile(unsigned id) {
    return id < ISSD_MAX_STADIUMS && s_present[id] ? &s_profiles[id] : NULL;
}

unsigned issd_stadium_generation(void) { return s_generation; }

const IssdStadiumAssets *issd_stadium_assets(unsigned id) {
    return id < ISSD_MAX_STADIUMS ? s_assets[id] : NULL;
}

static bool artwork_compile(const IssdModPack *pack,
                             const IssdStadiumProfile *profile,
                             IssdStadiumAssets **assets) {
    if (!profile->artwork[0]) return true;
    const char *slash = strrchr(pack->filepath, '/');
    const char *backslash = strrchr(pack->filepath, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    size_t directory_size = slash ? (size_t)(slash - pack->filepath + 1) : 0;
    char directory[1024], path[4096];
    if (directory_size >= sizeof directory) return false;
    memcpy(directory, pack->filepath, directory_size);
    directory[directory_size] = 0;
    if (!directory_size) strcpy(directory, ".");
    if (!issd_asset_resolve(directory, profile->artwork, path, sizeof path)) return false;
    if (!issd_stadium_assets_load(path, profile->base_layout, assets)) return false;
    return !(*assets)->geometry.length ||
        ((*assets)->geometry.length == profile->length_units && (*assets)->geometry.width == profile->width_units);
}

static bool assets_equal(const IssdStadiumAssets *left, const IssdStadiumAssets *right) {
    if (!left || !right) return left == right;
    if (left->write_count != right->write_count || left->palette_size != right->palette_size ||
        left->hd_count != right->hd_count || memcmp(&left->geometry,&right->geometry,sizeof left->geometry) ||
        memcmp(left->palette,right->palette,left->palette_size)) return false;
    for (size_t i = 0; i < left->write_count; ++i) {
        const IssdStadiumTileWrite *a = &left->writes[i], *b = &right->writes[i];
        if (a->word_destination != b->word_destination || a->size != b->size ||
            memcmp(a->data,b->data,a->size)) return false;
    }
    for (size_t i = 0; i < left->hd_count; ++i) {
        const IssdStadiumHdAsset *a = &left->hd[i], *b = &right->hd[i];
        if (a->key != b->key || strcmp(a->filename,b->filename) ||
            memcmp(a->content_digest,b->content_digest,32)) return false;
    }
    return true;
}

bool issd_stadium_rebuild_registry(void) {
    IssdStadiumProfile resolved[ISSD_MAX_STADIUMS] = {0};
    bool present[ISSD_MAX_STADIUMS] = {0};
    const IssdModPack *owners[ISSD_MAX_STADIUMS] = {0};
    IssdStadiumAssets *assets[ISSD_MAX_STADIUMS] = {0};
    for (int order = 0;; ++order) {
        int index = issd_mod_pack_at_order(order);
        if (index < 0) break;
        const IssdModPack *pack = issd_mod_get_pack(index);
        if (!pack) return false;
        for (int i = 0; i < pack->stadium_count; ++i) {
            const IssdModStadium *entry = &pack->stadiums[i];
            int id = entry->stadium_id;
            if (id < 0 || id >= ISSD_MAX_STADIUMS) continue;
            /* A winning legacy entry clears an earlier custom profile too. */
            present[id] = entry->has_profile;
            owners[id] = pack;
            memset(&resolved[id], 0, sizeof resolved[id]);
            if (entry->has_profile) resolved[id] = entry->profile;
        }
    }
    for (unsigned id = 0; id < ISSD_MAX_STADIUMS; ++id) {
        if (present[id] && !artwork_compile(owners[id],&resolved[id],&assets[id])) {
            for (unsigned cleanup = 0; cleanup < ISSD_MAX_STADIUMS; ++cleanup)
                issd_stadium_assets_free(assets[cleanup]);
            return false;
        }
    }
    bool changed = memcmp(present,s_present,sizeof present) || memcmp(resolved,s_profiles,sizeof resolved);
    for (unsigned id = 0; id < ISSD_MAX_STADIUMS; ++id) {
        if (!assets_equal(assets[id],s_assets[id])) {
            changed = true;
            issd_stadium_assets_free(s_assets[id]); s_assets[id] = assets[id];
        } else issd_stadium_assets_free(assets[id]);
    }
    if (changed) {
        memcpy(s_present, present, sizeof present);
        memcpy(s_profiles, resolved, sizeof resolved);
        ++s_generation;
    }
    return true;
}

void issd_stadium_gameplay_digest(uint8_t out[32]) {
    /* Stable little-endian encoding; names, paths and JSON order excluded. */
    uint8_t canonical[8 + ISSD_MAX_STADIUMS * 19] = {'I','S','S','D','S','T','D',1};
    size_t position = 8;
    for (unsigned id = 0; id < ISSD_MAX_STADIUMS; ++id) {
        const IssdStadiumProfile *profile = issd_stadium_profile(id);
        if (!profile) continue;
        canonical[position++] = (uint8_t)id;
        uint16_t words[9] = {profile->version, profile->base_layout,
                            profile->length_units, profile->width_units};
        memcpy(words + 4, profile->camera, sizeof profile->camera);
        for (unsigned i = 0; i < 9; ++i) {
            canonical[position++] = (uint8_t)words[i];
            canonical[position++] = (uint8_t)(words[i] >> 8);
        }
    }
    sha256_compute(canonical, position, out);
}

static FILE *s_trace;
static bool s_trace_initialized, s_trace_ppu_pending;
static uint64_t s_trace_tick;
static unsigned s_trace_generation;
static unsigned s_trace_frame;
void issd_stadium_trace_set_frame(unsigned frame) { s_trace_frame = frame; }

bool issd_stadium_trace_enabled(void) {
    if (!s_trace_initialized) {
        s_trace_initialized = true;
        const char *path = getenv("ISSD_STADIUM_TRACE");
        if (path && *path) {
            s_trace = fopen(path, "wb");
            if (!s_trace) fprintf(stderr, "[stadium] Cannot open trace: %s\n", path);
        }
    }
    return s_trace != NULL;
}

static uint16_t trace_word(const uint8_t *ram, unsigned address) {
    return (uint16_t)(ram[address] | (unsigned)ram[address+1] << 8);
}

static void trace_event(const uint8_t *ram, const char *kind, uint32_t pc) {
    fprintf(s_trace, "{\"kind\":\"%s\",\"pc\":%u,\"tick\":%llu,\"frame\":%u,"
            "\"generation\":%u,\"mode\":%u,\"logical_id\":%u,"
            "\"base_layout\":%u,\"length_units\":%u,\"width_units\":%u,"
            "\"score_home\":%u,\"score_away\":%u,\"submode\":%u,"
            "\"dma_queue_end\":%u,\"deferred_descriptors\":%u,"
            "\"defer_mode\":%u,\"defer_scene\":%u}\n",
            kind, (unsigned)pc, (unsigned long long)s_trace_tick, s_trace_frame,
            s_trace_generation, trace_word(ram, 0x70), trace_word(ram, 0x1fa2),
            trace_word(ram, 0x86), trace_word(ram, 0x12a2), trace_word(ram, 0x12a4),
            trace_word(ram, 0xda2), trace_word(ram, 0xea2), trace_word(ram, 0x72),
            trace_word(ram, 0x48), trace_word(ram, 0x130),
            trace_word(ram, 0x100), trace_word(ram, 0x1f00));
    fflush(s_trace);
}

void issd_stadium_trace_opcode(const uint8_t ram[0x20000], uint32_t pc) {
    if (!issd_stadium_trace_enabled()) return;
    ++s_trace_tick;
    const char *kind = NULL;
    switch (pc & 0x7fffff) {
        case 0x05a50a: ++s_trace_generation; kind = "scene_selection"; break;
        case 0x24e0b3: kind = "constructor_entry"; break;
        case 0x0bdb65: case 0x18f205: case 0x24d7ce:
            kind = "constructor_return"; break;
        case 0x24d6b2: case 0x24d6c5: kind = "replay_table_read"; break;
        case 0x0b8cec: kind = "palette_entry"; break;
        case 0x0b8dc0: kind = "palette_return"; break;
        case 0x0b8dc1: kind = "palette_copy_entry"; break;
        case 0x00bbe4: kind = "decompression_entry"; break;
        /* Original token decoding/copy or deinterleave has produced this
         * chunk before either synchronous hardware DMA or queued descriptor
         * construction. This is chunk readiness, not whole-scene completion. */
        case 0x00b8ba: case 0x00b90f: kind = "transfer_payload_ready"; break;
        case 0x0b8cc4: kind = "graphics_resources_return"; break;
        case 0x0b8ceb: kind = "map_resources_return"; break;
        case 0x00b518: kind = "deferred_resources_return"; break;
        case 0x00b909: kind = "dma_channel_return"; break;
        case 0x00b90d: kind = "dma_yield_return"; break;
        case 0x008db8: kind = "dma_batch_return"; break;
        case 0x0b85e3: case 0x0b86e9:
            kind = "streamer_entry"; s_trace_ppu_pending = true; break;
    }
    if (kind) trace_event(ram, kind, pc);
}

void issd_stadium_trace_ppu(const uint8_t ram[0x20000]) {
    if (!issd_stadium_trace_enabled() || !s_trace_ppu_pending) return;
    ++s_trace_tick;
    trace_event(ram, "ppu", 0);
    s_trace_ppu_pending = false;
}
