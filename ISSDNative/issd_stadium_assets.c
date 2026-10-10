#include "issd_stadium_assets.h"
#include "issd_asset_path.h"
#include "issd_json_internal.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "sha256.h"

#include "issd_stadium_allocations.h"
#define MAX_WRITES 256
#define MAX_HD 512
typedef struct LayerRecord {
    unsigned layer, pages_x, pages_y;
    char metatiles[256], world_map[256];
} LayerRecord;
typedef struct TileRecord {
    unsigned slot, offset_tiles, tile_count;
    char file[256];
} TileRecord;
typedef struct HdRecord { char key[32], file[256]; } HdRecord;
typedef struct Manifest {
    unsigned version, base;
    unsigned length, width, geometry_present;
    char allocation[64], palette[256];
    LayerRecord layers[2];
    TileRecord tiles[MAX_WRITES];
    HdRecord hd[MAX_HD];
    unsigned layer_count, tile_count, hd_count;
    bool has_palette;
} Manifest;

static const unsigned page_widths[8] = {11,11,12,12,12,11,11,13};

static bool integer(JsonReader *reader, unsigned *value) {
    json_skip_ws(reader);
    const char *p = reader->p;
    if (*p < '0' || *p > '9' || (*p == '0' && p[1] >= '0' && p[1] <= '9')) return false;
    unsigned number = 0;
    while (*p >= '0' && *p <= '9') {
        unsigned digit = (unsigned)(*p++-'0');
        if (number > (65535-digit)/10) return false;
        number = number*10+digit;
    }
    if (*p == '.' || *p == 'e' || *p == 'E') return false;
    *value = number; reader->p = p; return true;
}
static bool string(JsonReader *reader, char *value, size_t size) {
    char full[4096];
    if (!json_string(reader, full, sizeof full) || strlen(full) >= size) return false;
    strcpy(value, full); return true;
}
static bool layer_member(JsonReader *reader, const char *key, void *context) {
    LayerRecord *record = context;
    if (!strcmp(key,"layer")) return integer(reader,&record->layer);
    if (!strcmp(key,"pages_x")) return integer(reader,&record->pages_x);
    if (!strcmp(key,"pages_y")) return integer(reader,&record->pages_y);
    if (!strcmp(key,"metatiles")) return string(reader,record->metatiles,sizeof record->metatiles);
    if (!strcmp(key,"world_map")) return string(reader,record->world_map,sizeof record->world_map);
    return json_skip_value(reader);
}
static bool layer_element(JsonReader *reader, void *context) {
    Manifest *manifest = context;
    if (manifest->layer_count >= 2) return false;
    LayerRecord *record = &manifest->layers[manifest->layer_count++];
    record->layer = UINT_MAX;
    return json_object(reader,layer_member,record);
}
static bool tile_member(JsonReader *reader, const char *key, void *context) {
    TileRecord *record = context;
    if (!strcmp(key,"slot")) return integer(reader,&record->slot);
    if (!strcmp(key,"offset_tiles")) return integer(reader,&record->offset_tiles);
    if (!strcmp(key,"tile_count")) return integer(reader,&record->tile_count);
    if (!strcmp(key,"file")) return string(reader,record->file,sizeof record->file);
    return json_skip_value(reader);
}
static bool tile_element(JsonReader *reader, void *context) {
    Manifest *manifest = context;
    if (manifest->tile_count >= MAX_WRITES) return false;
    TileRecord *record = &manifest->tiles[manifest->tile_count++];
    record->slot = record->offset_tiles = record->tile_count = UINT_MAX;
    return json_object(reader,tile_member,record);
}
static bool hd_member(JsonReader *reader, const char *key, void *context) {
    HdRecord *record = context;
    if (!strcmp(key,"key")) return string(reader,record->key,sizeof record->key);
    if (!strcmp(key,"file")) return string(reader,record->file,sizeof record->file);
    return json_skip_value(reader);
}
static bool hd_element(JsonReader *reader, void *context) {
    Manifest *manifest = context;
    if (manifest->hd_count >= MAX_HD) return false;
    return json_object(reader,hd_member,&manifest->hd[manifest->hd_count++]);
}
static bool geometry_member(JsonReader *reader, const char *key, void *context) {
    Manifest *manifest = context;
    if (!strcmp(key,"length_units")) { manifest->geometry_present |= 1; return integer(reader,&manifest->length); }
    if (!strcmp(key,"width_units")) { manifest->geometry_present |= 2; return integer(reader,&manifest->width); }
    return json_skip_value(reader);
}
static bool manifest_member(JsonReader *reader, const char *key, void *context) {
    Manifest *manifest = context;
    if (!strcmp(key,"version")) return integer(reader,&manifest->version);
    if (!strcmp(key,"base_layout")) return integer(reader,&manifest->base);
    if (!strcmp(key,"geometry")) { manifest->geometry_present |= 4; return json_object(reader,geometry_member,manifest); }
    if (!strcmp(key,"allocation_profile")) return string(reader,manifest->allocation,sizeof manifest->allocation);
    if (!strcmp(key,"palette")) {
        manifest->has_palette = true;
        return string(reader,manifest->palette,sizeof manifest->palette);
    }
    if (!strcmp(key,"layers")) return json_array(reader,layer_element,manifest);
    if (!strcmp(key,"tiles")) return json_array(reader,tile_element,manifest);
    if (!strcmp(key,"hd")) return json_array(reader,hd_element,manifest);
    return json_skip_value(reader);
}
static bool read_exact(const char *directory, const char *name, uint8_t *bytes, size_t size) {
    char path[4096];
    if (!issd_asset_resolve(directory,name,path,sizeof path)) return false;
    FILE *file = fopen(path,"rb");
    if (!file) return false;
    bool valid = fread(bytes,1,size,file) == size && fgetc(file) == EOF && !ferror(file);
    if (fclose(file)) valid = false;
    return valid;
}
void issd_stadium_assets_free(IssdStadiumAssets *assets) {
    if (!assets) return;
    for (size_t i = 0; i < assets->write_count; ++i) free(assets->writes[i].data);
    free(assets->writes); free(assets->hd); free(assets);
}
static uint32_t word32(const uint8_t *source) {
    return source[0] | (uint32_t)source[1]<<8 | (uint32_t)source[2]<<16 | (uint32_t)source[3]<<24;
}
static bool hd_file(const char *directory, const HdRecord *record, IssdStadiumHdAsset *asset) {
    if (strlen(record->key) != 16 || !issd_asset_resolve(directory,record->file,
            asset->filename,sizeof asset->filename)) return false;
    asset->key = 0;
    for (unsigned i = 0; i < 16; ++i) {
        char character = record->key[i];
        unsigned digit;
        if (character >= '0' && character <= '9') digit = (unsigned)(character-'0');
        else if (character >= 'a' && character <= 'f') digit = (unsigned)(character-'a'+10);
        else if (character >= 'A' && character <= 'F') digit = (unsigned)(character-'A'+10);
        else return false;
        asset->key = (asset->key<<4)|digit;
    }
    size_t name_size = strlen(record->file);
    if (name_size < 4) return false;
    const char *suffix = record->file+name_size-4;
    if (suffix[0] != '.' || (suffix[1] != 'b' && suffix[1] != 'B') ||
        (suffix[2] != 'm' && suffix[2] != 'M') || (suffix[3] != 'p' && suffix[3] != 'P')) return false;
    FILE *file = fopen(asset->filename,"rb");
    if (!file) return false;
    uint8_t header[54];
    bool valid = fread(header,1,sizeof header,file) == sizeof header;
    if (valid) {
        uint32_t width = word32(header+18), height = word32(header+22);
        if (height & 0x80000000) height = 0-height;
        uint32_t pixels = word32(header+10);
        valid = header[0] == 'B' && header[1] == 'M' && word32(header+14) >= 40 &&
            header[26] == 1 && header[27] == 0 && header[28] == 32 && header[29] == 0 &&
            word32(header+30) == 0 && width && width <= 512 && width == height &&
            !(width&7) && pixels >= 54 && pixels <= 1024*1024;
        if (valid) {
            if (fseek(file,0,SEEK_END)) valid = false;
            else { long size = ftell(file); valid = size >= 0 && (uint64_t)size >= (uint64_t)pixels+width*height*4; }
        }
        if (valid) {
            size_t size = (size_t)width*height*4;
            uint8_t *data = malloc(size);
            valid = data && fseek(file,(long)pixels,SEEK_SET) == 0 && fread(data,1,size,file) == size;
            if (valid) sha256_compute(data,size,asset->content_digest);
            free(data);
        }
    }
    fclose(file); return valid;
}
bool issd_stadium_assets_load(const char *manifest_path, unsigned expected_base,
                              IssdStadiumAssets **output) {
    if (!manifest_path || !output || expected_base >= 8) return false;
    FILE *file = fopen(manifest_path,"rb");
    if (!file) return false;
    if (fseek(file,0,SEEK_END)) { fclose(file); return false; }
    long length = ftell(file);
    if (length <= 0 || length > 1024*1024 || fseek(file,0,SEEK_SET)) { fclose(file); return false; }
    char *text = malloc((size_t)length+1);
    if (!text) { fclose(file); return false; }
    bool valid = fread(text,1,(size_t)length,file) == (size_t)length;
    fclose(file); text[length] = 0;
    Manifest *manifest = calloc(1,sizeof *manifest);
    IssdStadiumAssets *assets = calloc(1,sizeof *assets);
    if (!manifest || !assets) { free(text); free(manifest); free(assets); return false; }
    manifest->base = UINT_MAX;
    JsonReader reader = {text,1,NULL,0};
    valid = valid && strlen(text) == (size_t)length && json_object(&reader,manifest_member,manifest);
    json_skip_ws(&reader);
    valid = valid && !*reader.p && manifest->version == 1 && manifest->base == expected_base &&
        !strcmp(manifest->allocation,"original-scenery-v1") && manifest->layer_count == 2;
    free(text);
    char directory[4096];
    const char *slash = strrchr(manifest_path,'/'), *backslash = strrchr(manifest_path,'\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    size_t directory_size = slash ? (size_t)(slash-manifest_path+1) : 0;
    if (directory_size >= sizeof directory) valid = false;
    else {
        memcpy(directory,manifest_path,directory_size); directory[directory_size] = 0;
        if (!directory_size) strcpy(directory,".");
    }
    unsigned seen = 0;
    for (unsigned i = 0; valid && i < 2; ++i) {
        LayerRecord *record = &manifest->layers[i];
        valid = record->layer < 2 && !(seen & (1u<<record->layer)) &&
            record->pages_x == page_widths[expected_base] && record->pages_y >= 1 &&
            record->pages_y <= 64/page_widths[expected_base];
        if (!valid) break;
        seen |= 1u<<record->layer;
        valid = read_exact(directory,record->metatiles,assets->geometry.metatiles[record->layer],8192) &&
            read_exact(directory,record->world_map,assets->geometry.world_maps[record->layer],4096);
        if (i && (record->pages_x != manifest->layers[0].pages_x ||
                  record->pages_y != manifest->layers[0].pages_y)) valid = false;
    }
    assets->geometry.base_layout = (uint16_t)expected_base;
    if (manifest->geometry_present) {
        static const unsigned lengths[8] = {1792,1856,1984,2048,1920,1920,1792,2176};
        static const unsigned widths[8] = {576,640,704,640,640,576,704,704};
        if (manifest->geometry_present != 7 || manifest->length < 1536 ||
            manifest->length > lengths[expected_base] || (manifest->length&31) ||
            manifest->width != widths[expected_base]) valid = false;
        assets->geometry.length = (uint16_t)manifest->length;
        assets->geometry.width = (uint16_t)manifest->width;
    }
    assets->geometry.stride = (uint16_t)(page_widths[expected_base]*64);
    if (valid && manifest->has_palette) {
        assets->palette_size = expected_base == 2 || expected_base == 6 ? 64 : 96;
        valid = read_exact(directory,manifest->palette,assets->palette,assets->palette_size);
        for (size_t i = 0; valid && i < assets->palette_size; i += 2)
            if ((assets->palette[i+1]&0x80) || (!(i%32) && (assets->palette[i] || assets->palette[i+1]))) valid = false;
    }
    assets->writes = calloc(manifest->tile_count ? manifest->tile_count : 1,sizeof *assets->writes);
    uint8_t occupied[0x1000] = {0};
    if (!assets->writes) valid = false;
    for (unsigned i = 0; valid && i < manifest->tile_count; ++i) {
        TileRecord *record = &manifest->tiles[i];
        valid = record->slot < 5 && issd_stadium_slots[expected_base][record->slot][1] &&
            record->offset_tiles < issd_stadium_slots[expected_base][record->slot][1] && record->tile_count >= 1 &&
            record->tile_count <= issd_stadium_slots[expected_base][record->slot][1]-record->offset_tiles;
        if (!valid) break;
        IssdStadiumTileWrite *write = &assets->writes[assets->write_count++];
        write->word_destination = (uint16_t)(issd_stadium_slots[expected_base][record->slot][0]+record->offset_tiles*16);
        write->size = record->tile_count*32;
        for (unsigned j = 0; valid && j < record->tile_count*16; ++j) {
            unsigned index = write->word_destination+j-0x4000;
            if (index >= sizeof occupied || occupied[index]) valid = false;
            else occupied[index] = 1;
        }
        if (!valid) break;
        write->data = malloc(write->size);
        valid = write->data && read_exact(directory,record->file,write->data,write->size);
    }
    assets->hd = calloc(manifest->hd_count ? manifest->hd_count : 1,sizeof *assets->hd);
    if (!assets->hd) valid = false;
    for (unsigned i = 0; valid && i < manifest->hd_count; ++i) {
        IssdStadiumHdAsset *asset = &assets->hd[assets->hd_count++];
        valid = hd_file(directory,&manifest->hd[i],asset);
        for (unsigned j = 0; valid && j < i; ++j)
            if (assets->hd[j].key == asset->key) valid = false;
    }
    free(manifest);
    if (!valid) { issd_stadium_assets_free(assets); return false; }
    issd_stadium_assets_free(*output); *output = assets; return true;
}
