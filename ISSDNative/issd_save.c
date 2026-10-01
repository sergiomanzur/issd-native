#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "issd_save.h"
#include "snes/snes.h"
#include "cpu_state.h"
#include "issd_bridge.h"
#include "common_cpu_infra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "issd_android.h"
#include "issd_widescreen.h"
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include "sha256.h"
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

extern uint8_t g_ram[0x20000];

#define SAVE_MAGIC 0x44535349 /* 'ISSD' */
#define SAVE_VERSION 2
#define SAVES_DIR "saves"
#define SAVE_PATH_MAX 2048
#define ENVELOPE_HEADER 192u
#define ENVELOPE_HASH_OFFSET 160u
#define MAX_PAYLOAD (16u * 1024u * 1024u)
static char s_directory[SAVE_PATH_MAX];
static uint8_t s_context[32];
static bool s_has_context;
static uint32_t s_gameplay_flags;
static char s_error[160];
static void (*s_load_callback)(void);

static bool Fail(const char *message) {
    snprintf(s_error, sizeof(s_error), "%s", message);
    return false;
}

const char *issd_save_error(void) { return s_error; }
void issd_save_set_load_callback(void (*callback)(void)) { s_load_callback = callback; }

static const char *GetSavesDir(char *buf, size_t size) {
    if (s_directory[0]) {
        snprintf(buf, size, "%s", s_directory);
        return buf;
    }
#ifdef ISSD_ANDROID
    const char *internal = issd_android_internal_dir();
    if (internal && internal[0]) {
        snprintf(buf, size, "%s/saves", internal);
        return buf;
    }
#endif
    /* The small no-context fallback retains its portable test/legacy layout.
     * Production installs an applied ROM context before accessing saves. */
    if (s_has_context) {
#ifdef _WIN32
        const char *appdata = getenv("LOCALAPPDATA");
        if (!appdata || !appdata[0]) appdata = getenv("APPDATA");
        if (appdata && appdata[0]) {
            snprintf(buf, size, "%s/ISSDNative/saves", appdata);
            return buf;
        }
#else
        const char *data = getenv("XDG_DATA_HOME");
        if (data && data[0] == '/') {
            snprintf(buf, size, "%s/issd-native/saves", data);
            return buf;
        }
        const char *user_home = getenv("HOME");
        if (user_home && user_home[0] == '/') {
            snprintf(buf, size, "%s/.local/share/issd-native/saves", user_home);
            return buf;
        }
#endif
        buf[0] = 0;
        return buf;
    }
    snprintf(buf, size, "%s", SAVES_DIR);
    return buf;
}

static bool MakeDir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return (st.st_mode & S_IFMT) == S_IFDIR;
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0700) == 0;
#endif
}

static bool MakeDirs(const char *path) {
    char copy[SAVE_PATH_MAX];
    size_t n = strlen(path);
    if (!n || n >= sizeof(copy)) return Fail("Invalid save directory");
    memcpy(copy, path, n + 1);
    for (size_t i = 1; i < n; ++i) {
        if (copy[i] == '/' || copy[i] == '\\') {
#ifdef _WIN32
            if (i == 2 && copy[1] == ':') continue;
            /* Preserve the server/share prefix of a UNC path. */
            if ((copy[0] == '\\' || copy[0] == '/') && (copy[1] == '\\' || copy[1] == '/')) {
                unsigned separators = 0;
                for (size_t j = 2; j < i; ++j) if (copy[j] == '/' || copy[j] == '\\') ++separators;
                if (separators < 1) continue;
            }
#endif
            char c = copy[i]; copy[i] = 0;
            bool ok = MakeDir(copy);
            copy[i] = c;
            if (!ok) return Fail("Cannot create save directory");
        }
    }
    return MakeDir(copy) || Fail("Cannot create save directory");
}

static bool EnsureSaveDir(void) {
    char dir[SAVE_PATH_MAX];
    GetSavesDir(dir, sizeof(dir));
    return MakeDirs(dir);
}

static void GetSlotPath(int slot_index, char *out_path, size_t max_len) {
    char dir[SAVE_PATH_MAX];
    GetSavesDir(dir, sizeof(dir));
    if (slot_index < 0) {
        snprintf(out_path, max_len, "%s/quicksave.sav", dir);
    } else {
        snprintf(out_path, max_len, "%s/slot_%d.sav", dir, slot_index);
    }
}

/* Move the PPU snapshot region between the live PPU and a slot. Absent a PPU
 * (unit tests, pre-init) the simulation half of the slot still round-trips. */
static void CopyVideoState(IssdSaveSlot *slot, bool to_ppu) {
    Ppu *ppu = g_snes ? g_snes->ppu : NULL;
    if (!ppu) return;
    if (to_ppu) {
        memcpy(ppu->cgram, slot->cgram, sizeof(slot->cgram));
        memcpy(ppu->oam, slot->oam, sizeof(slot->oam));
        memcpy(ppu->highOam, slot->high_oam, sizeof(slot->high_oam));
        memcpy(ppu->vram, slot->vram, sizeof(slot->vram));
    } else {
        memcpy(slot->cgram, ppu->cgram, sizeof(slot->cgram));
        memcpy(slot->oam, ppu->oam, sizeof(slot->oam));
        memcpy(slot->high_oam, ppu->highOam, sizeof(slot->high_oam));
        memcpy(slot->vram, ppu->vram, sizeof(slot->vram));
    }
}

static IssdSnapshotSaveFn s_save_snapshot_backend = NULL;
static IssdSnapshotLoadFn s_load_snapshot_backend = NULL;
static bool (*s_validate_snapshot_backend)(const void *, size_t);

void issd_save_set_snapshot_validator(bool (*validator)(const void *, size_t)) {
    s_validate_snapshot_backend = validator;
}

void issd_save_set_snapshot_backends(IssdSnapshotSaveFn save_fn, IssdSnapshotLoadFn load_fn) {
    s_save_snapshot_backend = save_fn;
    s_load_snapshot_backend = load_fn;
}

bool issd_save_init(void) {
    s_error[0] = 0;
    return EnsureSaveDir();
}

static bool RawSaveSlot(int slot_index, const char *label) {
    if (!EnsureSaveDir()) return false;
    char path[SAVE_PATH_MAX];
    GetSlotPath(slot_index, path, sizeof(path));

    if (s_save_snapshot_backend) {
        bool ok = s_save_snapshot_backend(path);
        if (ok) {
            printf("[Save] Snapshot saved to '%s' (Slot %d)\n", path, slot_index < 0 ? 0 : slot_index + 1);
        } else {
            fprintf(stderr, "[Save] Snapshot save failed for '%s'\n", path);
        }
        return ok;
    }

    IssdSaveSlot slot;
    memset(&slot, 0, sizeof(slot));

    slot.magic = SAVE_MAGIC;
    slot.version = SAVE_VERSION;
    slot.timestamp = (uint64_t)time(NULL);
    slot.frame_counter = snes_frame_counter;

    IssdMatchState ms;
    issd_bridge_update_state(&ms);

    slot.game_mode1 = ms.game_mode1;
    slot.game_mode2 = ms.game_mode2;
    slot.p1_team = ms.p1_team;
    slot.p2_team = ms.p2_team;
    slot.p1_score = ms.p1_score;
    slot.p2_score = ms.p2_score;
    slot.match_seconds_remaining = (uint16_t)(ms.timer_minutes * 60 + ms.timer_seconds);

    if (label && label[0]) {
        strncpy(slot.slot_label, label, sizeof(slot.slot_label) - 1);
    } else {
        snprintf(slot.slot_label, sizeof(slot.slot_label), "Save %d", slot_index);
    }

    /* Copy WRAM */
    memcpy(slot.wram, g_ram, sizeof(slot.wram));
    CopyVideoState(&slot, false);

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[Save] Failed to open '%s' for writing.\n", path);
        return false;
    }

    size_t written = fwrite(&slot, 1, sizeof(slot), f);
    fclose(f);

    if (written != sizeof(slot)) {
        fprintf(stderr, "[Save] Incomplete write to '%s'.\n", path);
        return false;
    }

    printf("[Save] Saved state to '%s' (%s - P1: %u vs P2: %u, Time: %02u:%02u)\n",
           path, slot.slot_label, slot.p1_score, slot.p2_score,
           slot.match_seconds_remaining / 60, slot.match_seconds_remaining % 60);
    return true;
}

static bool RawLoadPath(const char *path) {
    if (s_load_snapshot_backend) {
        bool ok = s_load_snapshot_backend(path);
        if (ok) {
            issd_widescreen_reset();
            printf("[Save] Snapshot loaded from '%s'\n", path);
        } else {
            printf("[Save] Snapshot load failed for '%s'\n", path);
        }
        return ok;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[Save] Slot file '%s' not found.\n", path);
        return false;
    }

    IssdSaveSlot slot;
    size_t read_bytes = fread(&slot, 1, sizeof(slot), f);
    fclose(f);

    if (read_bytes != sizeof(slot) || slot.magic != SAVE_MAGIC || slot.version != SAVE_VERSION) {
        fprintf(stderr, "[Save] Invalid save file header in '%s'.\n", path);
        return false;
    }

    /* Restore WRAM */
    memcpy(g_ram, slot.wram, sizeof(slot.wram));
    CopyVideoState(&slot, true);
    snes_frame_counter = slot.frame_counter;
    issd_widescreen_reset();

    printf("[Save] Loaded state from '%s' (%s - P1 Score: %u, P2 Score: %u)\n",
           path, slot.slot_label, slot.p1_score, slot.p2_score);
    return true;
}

/* Envelope v1 is explicitly little endian. The first 160 bytes are metadata,
 * followed by a SHA-256 digest, then the unchanged runner snapshot payload.
 * No host structure is written to disk. Runtime schema 1 delegates supported
 * raw payload versions to the transactional runner loader. */
typedef struct {
    uint8_t *bytes;
    size_t size;
    uint64_t generation;
    int source;
} CheckedSave;

static uint32_t Read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t Read64(const uint8_t *p) {
    return Read32(p) | ((uint64_t)Read32(p + 4) << 32);
}
static void Write32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i));
}
static void Write64(uint8_t *p, uint64_t n) {
    Write32(p, (uint32_t)n); Write32(p + 4, (uint32_t)(n >> 32));
}

bool issd_save_set_directory(const char *directory) {
    s_error[0] = 0;
    if (!directory || !directory[0] || strlen(directory) > SAVE_PATH_MAX - 192)
        return Fail("Invalid save directory");
    if (!MakeDirs(directory)) return false;
    snprintf(s_directory, sizeof(s_directory), "%s", directory);
    return true;
}

void issd_save_set_context(const uint8_t *base, size_t base_size,
                          const uint8_t *effective, size_t effective_size,
                          uint32_t gameplay_flags) {
    s_error[0] = 0;
    s_has_context = false;
    if (!base || !base_size || !effective || !effective_size) {
        Fail("Save context unavailable");
        return;
    }
    uint8_t canonical[76] = {'I','S','S','D','C','T','X',1};
    sha256_compute(base, base_size, canonical + 8);
    sha256_compute(effective, effective_size, canonical + 40);
    Write32(canonical + 72, gameplay_flags);
    sha256_compute(canonical, sizeof(canonical), s_context);
    s_gameplay_flags = gameplay_flags;
    s_has_context = true;
}

/* Reserve a same-directory name exclusively. Two game processes must never
 * truncate each other's captured payload, publish candidate, or load staging
 * file. Stale files are neither reused nor considered committed generations. */
static bool TemporaryPath(const char *path, const char *purpose, char *out) {
    static unsigned long sequence;
    char candidate[SAVE_PATH_MAX];
    out[0] = 0;
#ifdef _WIN32
    unsigned long pid = (unsigned long)_getpid();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        int n = snprintf(candidate, sizeof(candidate), "%s.%s.%lu.%lu.tmp", path, purpose, pid, ++sequence);
        if (n < 0 || n >= SAVE_PATH_MAX) return Fail("Save path too long");
#ifdef _WIN32
        int fd = _open(candidate, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            if (_close(fd) == 0) { memcpy(out, candidate, (size_t)n + 1); return true; }
            remove(candidate); return Fail("Temporary save close failed");
        }
#else
        int fd = open(candidate, O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (fd >= 0) {
            if (close(fd) == 0) { memcpy(out, candidate, (size_t)n + 1); return true; }
            remove(candidate); return Fail("Temporary save close failed");
        }
#endif
        if (errno != EEXIST) return Fail("Cannot create temporary save file");
    }
    return Fail("Cannot reserve temporary save file");
}

static bool CampaignPaths(char paths[3][SAVE_PATH_MAX], bool create) {
    if (!s_has_context) return Fail("Save context unavailable");
    char root[SAVE_PATH_MAX], directory[SAVE_PATH_MAX], hex[65];
    GetSavesDir(root, sizeof(root));
    if (!root[0]) return Fail("Save directory unavailable");
    for (unsigned i = 0; i < 32; ++i) snprintf(hex + 2 * i, 3, "%02x", s_context[i]);
    int n = snprintf(directory, sizeof(directory), "%s/campaigns/%s", root, hex);
    if (n < 0 || n > SAVE_PATH_MAX - 64) return Fail("Save path too long");
    if (create && !MakeDirs(directory)) return false;
    snprintf(paths[0], SAVE_PATH_MAX, "%s/campaign.sav", directory);
    snprintf(paths[1], SAVE_PATH_MAX, "%s/campaign.1.sav", directory);
    snprintf(paths[2], SAVE_PATH_MAX, "%s/campaign.2.sav", directory);
    return true;
}

static bool ReadBounded(const char *path, size_t limit, uint8_t **bytes, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return Fail("Save file unavailable");
    bool ok = fseek(f, 0, SEEK_END) == 0;
    long length = ok ? ftell(f) : -1;
    ok = length > 0 && (uint64_t)length <= limit && fseek(f, 0, SEEK_SET) == 0;
    uint8_t *data = ok ? malloc((size_t)length) : NULL;
    if (!data) ok = false;
    if (ok) ok = fread(data, 1, (size_t)length, f) == (size_t)length && fgetc(f) == EOF && !ferror(f);
    if (fclose(f)) ok = false;
    if (!ok) { free(data); return Fail("Invalid or oversized save file"); }
    *bytes = data; *size = (size_t)length;
    return true;
}

static bool EnvelopeDigest(const uint8_t *bytes, size_t size, uint8_t digest[32]) {
    if (size < ENVELOPE_HEADER) return false;
    size_t payload_size = size - ENVELOPE_HEADER;
    uint8_t *input = malloc(ENVELOPE_HASH_OFFSET + payload_size);
    if (!input) return Fail("Cannot allocate save validation buffer");
    memcpy(input, bytes, ENVELOPE_HASH_OFFSET);
    memcpy(input + ENVELOPE_HASH_OFFSET, bytes + ENVELOPE_HEADER, payload_size);
    sha256_compute(input, ENVELOPE_HASH_OFFSET + payload_size, digest);
    free(input);
    return true;
}

static bool ValidateEnvelope(const uint8_t *bytes, size_t size) {
    if (size < ENVELOPE_HEADER || memcmp(bytes, "ISCE", 4)) return Fail("Invalid save envelope");
    if (Read32(bytes + 4) != 1 || Read32(bytes + 8) != 1 || Read32(bytes + 12) != ENVELOPE_HEADER)
        return Fail("Unsupported save version");
    uint64_t payload = Read64(bytes + 16);
    if (!payload || payload > MAX_PAYLOAD || payload != size - ENVELOPE_HEADER || !Read64(bytes + 24))
        return Fail("Invalid save length or generation");
    if (Read32(bytes + 72) > 1 || !memchr(bytes + 80, 0, 64)) return Fail("Invalid save metadata");
    /* Display text is bounded and cannot contain control characters. */
    for (unsigned i = 80; i < 144 && bytes[i]; ++i)
        if (bytes[i] < 32 || bytes[i] == 127) return Fail("Invalid save metadata");
    for (unsigned i = 144; i < 160; ++i) if (bytes[i]) return Fail("Unsupported save metadata");
    uint8_t digest[32];
    if (!EnvelopeDigest(bytes, size, digest)) return false;
    if (memcmp(digest, bytes + ENVELOPE_HASH_OFFSET, 32)) return Fail("Save integrity check failed");
    if (!s_has_context || memcmp(bytes + 40, s_context, 32) || Read32(bytes + 76) != s_gameplay_flags)
        return Fail("Save is incompatible with applied gameplay data");
    if (s_validate_snapshot_backend &&
        !s_validate_snapshot_backend(bytes + ENVELOPE_HEADER, size - ENVELOPE_HEADER))
        return Fail("Invalid snapshot payload");
    return true;
}

static bool ReadEnvelope(const char *path, CheckedSave *out) {
    memset(out, 0, sizeof(*out));
    if (!ReadBounded(path, ENVELOPE_HEADER + MAX_PAYLOAD, &out->bytes, &out->size)) return false;
    if (!ValidateEnvelope(out->bytes, out->size)) {
        free(out->bytes); memset(out, 0, sizeof(*out)); return false;
    }
    out->generation = Read64(out->bytes + 24);
    return true;
}

static bool FlushFile(FILE *f) {
    if (fflush(f)) return false;
#ifdef _WIN32
    intptr_t native = _get_osfhandle(_fileno(f));
    return native != -1 && FlushFileBuffers((HANDLE)native) != 0;
#else
    return fsync(fileno(f)) == 0;
#endif
}

static bool WriteDurable(const char *path, const uint8_t *bytes, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return Fail("Cannot create temporary save file");
    bool ok = fwrite(bytes, 1, size, f) == size && FlushFile(f);
    if (fclose(f)) ok = false;
    return ok || Fail("Save write or flush failed");
}

static bool AtomicReplace(const char *candidate, const char *destination) {
#ifdef _WIN32
    if (!MoveFileExA(candidate, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return Fail("Atomic save replacement failed");
#else
    if (rename(candidate, destination)) return Fail("Atomic save replacement failed");
    char directory[SAVE_PATH_MAX];
    snprintf(directory, sizeof(directory), "%s", destination);
    char *slash = strrchr(directory, '/');
    if (slash) *slash = 0; else snprintf(directory, sizeof(directory), ".");
    int fd = open(directory, O_RDONLY);
    if (fd < 0) return Fail("Save directory sync failed");
    bool ok = fsync(fd) == 0;
    if (close(fd)) ok = false;
    if (!ok) return Fail("Save directory sync failed");
#endif
    return true;
}

static bool Publish(const char *path, const CheckedSave *save) {
    char temporary[SAVE_PATH_MAX];
    if (!TemporaryPath(path, "publish", temporary)) return false;
    if (!WriteDurable(temporary, save->bytes, save->size)) { remove(temporary); return false; }
    CheckedSave checked;
    bool ok = ReadEnvelope(temporary, &checked);
    if (ok) free(checked.bytes);
    if (ok) ok = AtomicReplace(temporary, path);
    if (!ok) remove(temporary);
    return ok;
}

static bool CreateEnvelope(const char *path, const char *label, uint64_t generation,
                           bool campaign, CheckedSave *out) {
    if (!s_save_snapshot_backend) return Fail("Full snapshot backend unavailable");
    char payload_path[SAVE_PATH_MAX];
    if (!TemporaryPath(path, "capture", payload_path)) return false;
    if (!s_save_snapshot_backend(payload_path)) {
        remove(payload_path); return Fail("Snapshot capture failed");
    }
    uint8_t *payload = NULL; size_t length = 0;
    bool ok = ReadBounded(payload_path, MAX_PAYLOAD, &payload, &length);
    remove(payload_path);
    if (!ok) return false;
    memset(out, 0, sizeof(*out));
    out->size = ENVELOPE_HEADER + length;
    out->bytes = calloc(1, out->size);
    if (!out->bytes) { free(payload); return Fail("Cannot allocate save envelope"); }
    uint8_t *h = out->bytes;
    memcpy(h, "ISCE", 4); Write32(h + 4, 1); Write32(h + 8, 1); Write32(h + 12, ENVELOPE_HEADER);
    Write64(h + 16, length); Write64(h + 24, generation); Write64(h + 32, (uint64_t)time(NULL));
    memcpy(h + 40, s_context, 32); Write32(h + 72, campaign ? 1 : 0); Write32(h + 76, s_gameplay_flags);
    /* The verified checkpoint predicate supplies the label. No guessed RAM
     * round/team offsets are persisted as apparently authoritative metadata. */
    if (!label || !label[0]) label = campaign ? "Campaign checkpoint" : "Manual state";
    for (unsigned i = 0; i < 63 && label[i]; ++i) {
        unsigned char c = (unsigned char)label[i];
        h[80 + i] = c < 32 || c == 127 ? ' ' : c;
    }
    memcpy(h + ENVELOPE_HEADER, payload, length); free(payload);
    if (!EnvelopeDigest(h, out->size, h + ENVELOPE_HASH_OFFSET)) {
        free(out->bytes); memset(out, 0, sizeof(*out)); return false;
    }
    if (!ValidateEnvelope(h, out->size)) {
        free(out->bytes); memset(out, 0, sizeof(*out)); return false;
    }
    out->generation = generation;
    return true;
}

static int GatherCampaign(const char paths[3][SAVE_PATH_MAX], CheckedSave saves[3]) {
    int count = 0;
    for (int i = 0; i < 3; ++i) {
        CheckedSave candidate;
        if (ReadEnvelope(paths[i], &candidate)) {
            if (Read32(candidate.bytes + 72) != 1) { free(candidate.bytes); continue; }
            candidate.source = i;
            int j = count++;
            while (j > 0 && saves[j - 1].generation < candidate.generation) {
                saves[j] = saves[j - 1]; --j;
            }
            saves[j] = candidate;
        }
    }
    return count;
}

static void FreeSaves(CheckedSave *saves, int count) {
    for (int i = 0; i < count; ++i) free(saves[i].bytes);
}

#ifdef _WIN32
typedef HANDLE CampaignLock;
#else
typedef int CampaignLock;
#endif

static bool AcquireCampaignLock(const char *primary, CampaignLock *lock) {
    char path[SAVE_PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s.lock", primary);
    if (n < 0 || n >= SAVE_PATH_MAX) return Fail("Save path too long");
    /* Keep the zero-length lock inode/name: removing it at unlock could let
     * another process lock a different inode while an existing holder runs.
     * The operating system releases the actual lock if the process exits. */
#ifdef _WIN32
    *lock = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (*lock == INVALID_HANDLE_VALUE) return Fail("Campaign save already in progress or unavailable");
#else
    *lock = open(path, O_CREAT | O_RDWR, 0600);
    if (*lock < 0) return Fail("Campaign save lock unavailable");
    struct flock operation;
    memset(&operation, 0, sizeof(operation));
    operation.l_type = F_WRLCK;
    operation.l_whence = SEEK_SET;
    if (fcntl(*lock, F_SETLK, &operation) < 0) {
        close(*lock); return Fail("Campaign save already in progress or unavailable");
    }
#endif
    return true;
}

static void ReleaseCampaignLock(CampaignLock lock) {
#ifdef _WIN32
    CloseHandle(lock);
#else
    close(lock);
#endif
}

bool issd_save_campaign(const char *label) {
    s_error[0] = 0;
    char paths[3][SAVE_PATH_MAX];
    if (!CampaignPaths(paths, true)) return false;
    CampaignLock lock;
    if (!AcquireCampaignLock(paths[0], &lock)) return false;
    CheckedSave prior[3];
    int count = GatherCampaign(paths, prior);
    uint64_t generation = count ? prior[0].generation + 1 : 1;
    if (!generation) {
        FreeSaves(prior, count); ReleaseCampaignLock(lock);
        return Fail("Save generation exhausted");
    }
    CheckedSave candidate;
    bool ok = CreateEnvelope(paths[0], label, generation, true, &candidate);
    if (ok) {
        /* Stage and validate the complete new envelope BEFORE touching history.
         * All prior generations are held in memory, so interrupted backup
         * publication cannot destroy the primary or alias a rotation source. */
        char temp[SAVE_PATH_MAX] = {0};
        ok = TemporaryPath(paths[0], "publish", temp) && WriteDurable(temp, candidate.bytes, candidate.size);
        CheckedSave validation;
        if (ok) { ok = ReadEnvelope(temp, &validation); if (ok) free(validation.bytes); }
        int unique[2], retained = 0;
        for (int i = 0; i < count && retained < 2; ++i)
            if (!retained || prior[i].generation != prior[unique[retained - 1]].generation) unique[retained++] = i;
        /* Oldest first; the valid primary is never removed before commit. */
        if (ok && retained > 1) ok = Publish(paths[2], &prior[unique[1]]);
        if (ok && retained > 0) ok = Publish(paths[1], &prior[unique[0]]);
        if (ok) ok = AtomicReplace(temp, paths[0]);
        if (!ok && temp[0]) remove(temp);
        free(candidate.bytes);
    }
    FreeSaves(prior, count);
    ReleaseCampaignLock(lock);
    if (ok) s_error[0] = 0;
    return ok;
}

static void FormatInfo(const CheckedSave *save, char *out, size_t size) {
    char when[32] = "Saved";
    uint64_t timestamp = Read64(save->bytes + 32);
    if (timestamp <= (uint64_t)INT64_MAX) {
        time_t t = (time_t)timestamp;
        struct tm *tm_info = localtime(&t);
        if (tm_info) strftime(when, sizeof(when), "%Y-%m-%d %H:%M", tm_info);
    }
    snprintf(out, size, "%s (%s)", (const char *)save->bytes + 80, when);
}

bool issd_save_continue_info(char *out_info, size_t max_len) {
    s_error[0] = 0;
    if (!out_info || !max_len) return Fail("Invalid save information buffer");
    out_info[0] = 0;
    char paths[3][SAVE_PATH_MAX];
    if (!CampaignPaths(paths, false)) { snprintf(out_info, max_len, "%s", s_error); return false; }
    CheckedSave saves[3]; int count = GatherCampaign(paths, saves);
    if (!count) {
        snprintf(out_info, max_len, "No recoverable campaign save");
        return Fail("No recoverable campaign save");
    }
    FormatInfo(&saves[0], out_info, max_len);
    FreeSaves(saves, count); s_error[0] = 0;
    return true;
}

static bool RestoreEnvelope(const char *original_path, const CheckedSave *save) {
    if (!s_load_snapshot_backend) return Fail("Full snapshot backend unavailable");
    char payload_path[SAVE_PATH_MAX];
    if (!TemporaryPath(original_path, "load", payload_path)) return false;
    bool ok = WriteDurable(payload_path, save->bytes + ENVELOPE_HEADER, save->size - ENVELOPE_HEADER);
    if (ok) ok = s_load_snapshot_backend(payload_path);
    remove(payload_path);
    if (!ok) return Fail("Snapshot restore failed");
    issd_widescreen_reset();
    if (s_load_callback) s_load_callback();
    return true;
}

bool issd_save_continue(void) {
    s_error[0] = 0;
    char paths[3][SAVE_PATH_MAX];
    if (!CampaignPaths(paths, false)) return false;
    CheckedSave saves[3]; int count = GatherCampaign(paths, saves);
    bool ok = false, recovered = false;
    for (int i = 0; i < count; ++i) {
        if (RestoreEnvelope(paths[saves[i].source], &saves[i])) {
            ok = true; recovered = saves[i].source != 0 || i != 0; break;
        }
    }
    FreeSaves(saves, count);
    if (!ok) return Fail("No recoverable campaign save");
    snprintf(s_error, sizeof(s_error), "%s", recovered ? "Recovered previous autosave" : "");
    return true;
}

static bool ValidSlot(int slot) {
    return (slot >= -1 && slot < ISSD_MAX_SAVE_SLOTS) || Fail("Invalid save slot");
}

static void ExistingSlotPath(int slot, char *out) {
    GetSlotPath(slot, out, SAVE_PATH_MAX);
    struct stat st;
    if (stat(out, &st) == 0) return;
    if (errno != ENOENT) return;
    if (slot < 0) snprintf(out, SAVE_PATH_MAX, "saves/quicksave.sav");
    else snprintf(out, SAVE_PATH_MAX, "saves/slot_%d.sav", slot);
}

static uint32_t FileMagic(const char *path) {
    uint8_t bytes[4] = {0};
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(bytes, 1, sizeof(bytes), f); fclose(f);
    return n == 4 ? Read32(bytes) : 0;
}

bool issd_save_is_legacy(int slot) {
    if (!ValidSlot(slot)) return false;
    char path[SAVE_PATH_MAX]; ExistingSlotPath(slot, path);
    uint32_t magic = FileMagic(path);
    return magic == 0x52544c53u || magic == SAVE_MAGIC;
}

bool issd_save_to_slot(int slot, const char *label) {
    s_error[0] = 0;
    if (!ValidSlot(slot)) return false;
    if (!s_has_context) {
        bool ok = RawSaveSlot(slot, label);
        return ok || Fail("Snapshot save failed");
    }
    if (!EnsureSaveDir()) return false;
    char path[SAVE_PATH_MAX]; GetSlotPath(slot, path, sizeof(path));
    CheckedSave previous, candidate;
    uint64_t generation = 1;
    if (ReadEnvelope(path, &previous)) { generation = previous.generation + 1; free(previous.bytes); }
    if (!generation) return Fail("Save generation exhausted");
    if (!CreateEnvelope(path, label, generation, false, &candidate)) return false;
    bool ok = Publish(path, &candidate); free(candidate.bytes);
    if (ok) s_error[0] = 0;
    return ok;
}

bool issd_load_from_slot_confirmed(int slot, bool allow_legacy) {
    s_error[0] = 0;
    if (!ValidSlot(slot)) return false;
    char path[SAVE_PATH_MAX]; ExistingSlotPath(slot, path);
    uint32_t magic = FileMagic(path);
    if (magic == 0x45435349u) {
        CheckedSave save;
        if (!ReadEnvelope(path, &save)) return false;
        bool ok = RestoreEnvelope(path, &save); free(save.bytes);
        if (ok) s_error[0] = 0;
        return ok;
    }
    if (magic != SAVE_MAGIC && magic != 0x52544c53u) return Fail("Invalid or missing save file");
    if (s_has_context && !allow_legacy) return Fail("Compatibility unknown: confirm legacy load");
    if (!RawLoadPath(path)) return Fail("Legacy snapshot restore failed");
    if (s_load_callback) s_load_callback();
    return true;
}

bool issd_load_from_slot(int slot) { return issd_load_from_slot_confirmed(slot, false); }
bool issd_save_quick(void) { return issd_save_to_slot(-1, "QuickSave"); }
bool issd_load_quick(void) { return issd_load_from_slot(-1); }

bool issd_save_get_info(int slot, char *out_info, size_t max_len) {
    s_error[0] = 0;
    if (!out_info || !max_len) return Fail("Invalid save information buffer");
    if (!ValidSlot(slot)) { snprintf(out_info, max_len, "%s", s_error); return false; }
    char path[SAVE_PATH_MAX]; ExistingSlotPath(slot, path);
    uint32_t magic = FileMagic(path);
    if (magic == 0x45435349u) {
        CheckedSave save;
        if (!ReadEnvelope(path, &save)) { snprintf(out_info, max_len, "%s", s_error); return false; }
        FormatInfo(&save, out_info, max_len); free(save.bytes); s_error[0] = 0;
        return true;
    }
    if (magic != SAVE_MAGIC && magic != 0x52544c53u) {
        snprintf(out_info, max_len, "%s", magic ? "Corrupted Slot" : "Empty Slot"); return false;
    }
    snprintf(out_info, max_len, "Compatibility unknown (legacy %s)", slot < 0 ? "quicksave" : "slot");
    return true;
}
