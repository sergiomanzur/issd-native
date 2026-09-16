#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#if !defined(__ANDROID__) && !defined(ISSD_ANDROID)
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include "types.h"
#include "snes/snes.h"
#include "snes/ppu.h"
#include "snes/apu.h"
#include "snes/dsp.h"
#include "snes/interp_bridge.h"
#include "common_rtl.h"
#include "common_cpu_infra.h"
#include "cpu_state.h"
#include "spc_player.h"
#include "issd_bridge.h"
#include "issd_config.h"
#include "issd_save.h"
#include "issd_mod.h"
#include "issd_menu.h"
#include "widescreen.h"
#include "issd_widescreen.h"
#include "issd_hd.h"
#include "issd_touch.h"
#include "issd_script.h"
#include "issd_android.h"
#include "launcher_picker.h"
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#endif
#include "issd_frame_pacing.h"
#include "issd_symbols.h"

#define DEFAULT_ROM_PATH "International Superstar Soccer Deluxe (USA).sfc"
#define DEFAULT_WINDOW_WIDTH  768
#define DEFAULT_WINDOW_HEIGHT 672
#define SNES_WIDTH    256
#define SNES_HEIGHT   224
#define MAX_WS_WIDTH  (SNES_WIDTH + 2 * kWsExtraMax) /* 256 + 190 = 446 */

/* Global SNESRecomp widescreen symbols */
bool g_ws_active = true;
int g_ws_extra = 71;

/* Global SNESRecomp host definitions */
SpcPlayer *g_spc_player = NULL;
static SDL_mutex *g_apu_mutex = NULL;

void RtlApuLock(void) {
    if (g_apu_mutex) SDL_LockMutex(g_apu_mutex);
}

void RtlApuUnlock(void) {
    if (g_apu_mutex) SDL_UnlockMutex(g_apu_mutex);
}

#ifdef _WIN32
#include <windows.h>
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS *ep) {
    fprintf(stderr, "[CRASH] Exception Code: 0x%08lX at Address: %p\n",
            ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
#include <signal.h>
#include <unistd.h>
/* Async-signal-safe: write(2) only, no stdio, no exit(). Restoring the
 * default handler and re-raising preserves the core dump and the real
 * exit status instead of masking the fault. */
static void CrashSignalHandler(int sig) {
    const char *name = sig == SIGSEGV ? "[CRASH] SIGSEGV\n"
                     : sig == SIGBUS  ? "[CRASH] SIGBUS\n"
                     : sig == SIGFPE  ? "[CRASH] SIGFPE\n"
                     : sig == SIGILL  ? "[CRASH] SIGILL\n"
                                      : "[CRASH] fatal signal\n";
    ssize_t written = write(2, name, strlen(name));
    (void)written;
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

void Die(const char *msg) {
    fprintf(stderr, "[FATAL ERROR] %s\n", msg ? msg : "Unknown error");
    fflush(stderr);
    exit(1);
}

void debug_on_wram_write_byte(uint32 addr, uint8 old_val, uint8 new_val) {
    (void)addr; (void)old_val; (void)new_val;
}

void debug_on_wram_write_word(uint32 addr, uint16 old_val, uint16 new_val) {
    (void)addr; (void)old_val; (void)new_val;
}

void debug_on_block_enter(uint32_t pc, uint32_t a, uint32_t x, uint32_t y) {
    (void)pc; (void)a; (void)x; (void)y;
}

static int g_auto_start_frame = -1;
/* Replay controls. Menu automation cannot reliably reach a given pitch
 * position, so widescreen work banks a state interactively and reinstalls it
 * here: -1 disables, otherwise the frame the quicksave slot is written after
 * or restored before. */
/* --dump-frames A:B writes every frame in the range to f_NNNNN.bmp.
 * --screenshot only ever captures the final frame, which cannot show motion,
 * so there was no way to check that an edge player animates rather than
 * holding a pose. Consecutive frames are the only evidence that settles it. */
static const char *g_script_path = NULL;
static const char *g_hd_pack_dir  = NULL;   /* --hd-pack DIR   */
static const char *g_hd_dump_dir  = NULL;   /* --dump-tiles DIR */
static unsigned    g_hd_dump_from = 0;      /* --dump-tiles-from N */

/* Relaunch so a new cartridge image is built from scratch.
 *
 * Mod packs patch the ROM image once, before the engine boots, and the
 * game caches roster data as a match loads. Switching packs mid-session
 * therefore does nothing visible until something reloads, which is
 * confusing: the menu says one pack and the pitch shows another. Starting
 * the process again is the honest way to apply it.
 *
 * The original command line is reused so --rom, --config and friends
 * survive the restart. */
static int    g_argc;
static char **g_argv;

static const char *ResolveConfigPath(char *out, size_t out_size, const char *cli_path);

static int g_dump_first = -1, g_dump_last = -1;
static int g_save_state_frame = -1;
static int g_load_state_frame = -1;
static uint32_t g_pixel_buffer[MAX_WS_WIDTH * SNES_HEIGHT];
static uint32_t g_pad1_state = 0;
static uint32_t g_pad2_state = 0;
static bool g_running = true;
static bool g_restart_requested = false;
static bool g_headless = false;
static int g_target_frames = -1;
static const char *g_screenshot_path = NULL;
static const char *g_dump_state_path = NULL;

void issd_request_quit(void) {
    g_running = false;
}

void issd_restart_application(void) {
    char cfg_path[1024];
    issd_config_save(&g_issd_config, ResolveConfigPath(cfg_path, sizeof(cfg_path), NULL));
#if defined(ISSD_ANDROID) || defined(__ANDROID__)
    /* On Android, execv("/proc/self/exe") or exit(0) crashes or destroys the JVM/Activity.
     * Perform an in-process soft reset instead. */
    g_restart_requested = true;
    issd_menu_close();
#elif defined(_WIN32)
    char exe[1024];
    if (GetModuleFileNameA(NULL, exe, sizeof(exe))) {
        /* Quote every argument: paths here routinely contain spaces. */
        char cmd[4096];
        int n = snprintf(cmd, sizeof(cmd), "\"%s\"", exe);
        for (int i = 1; i < g_argc && n > 0 && n < (int)sizeof(cmd); i++)
            n += snprintf(cmd + n, sizeof(cmd) - n, " \"%s\"", g_argv[i]);
        STARTUPINFOA si; PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            exit(0);
        }
    }
    fprintf(stderr, "[Restart] Could not relaunch; exiting instead.\n");
    exit(0);
#else
    /* execv replaces this process, so nothing after it runs on success. */
    if (g_argv) execv("/proc/self/exe", g_argv);
    fprintf(stderr, "[Restart] Could not relaunch; exiting instead.\n");
    exit(0);
#endif
}
static const uint8_t *g_rom_data;
static size_t g_rom_size;

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BmpFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BmpInfoHeader;
#pragma pack(pop)

static bool SaveBmp(const char *path, const uint32_t *pixels, int width, int height) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    BmpFileHeader fh = {
        .bfType = 0x4D42, /* "BM" */
        .bfSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + (uint32_t)(width * height * 4),
        .bfReserved1 = 0,
        .bfReserved2 = 0,
        .bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader),
    };

    BmpInfoHeader ih = {
        .biSize = sizeof(BmpInfoHeader),
        .biWidth = width,
        .biHeight = -height, /* Top-down DIB */
        .biPlanes = 1,
        .biBitCount = 32,
        .biCompression = 0, /* BI_RGB */
        .biSizeImage = (uint32_t)(width * height * 4),
        .biXPelsPerMeter = 2835,
        .biYPelsPerMeter = 2835,
        .biClrUsed = 0,
        .biClrImportant = 0,
    };

    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fwrite(pixels, 4, (size_t)(width * height), f);
    fclose(f);
    return true;
}

/* Save what the window would show, replacement tiles included.
 *
 * Replacements are composited into the scaled buffer, so a capture taken
 * from the native frame would miss them entirely - which is exactly the
 * thing a capture is usually taken to check. */
static int g_capture_scale = 4;
static bool SaveFrame(const char *path, const uint32_t *native, int w, int h);

/* Audio parameters */
#define AUDIO_FREQ 44100
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES 512

static int g_audio_frames_per_block = 0;
static int16_t s_audio_block[1024 * 2];
static int s_audio_block_avail = 0;
static int s_audio_block_pos = 0;

static void SDLCALL SdlAudioCallback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int samples_needed = len / (sizeof(int16_t) * AUDIO_CHANNELS);
    int16_t *out = (int16_t *)stream;

    if (g_audio_frames_per_block <= 0) {
        g_audio_frames_per_block = (534 * AUDIO_FREQ + 32040 / 2) / 32040;
    }

    int written = 0;
    while (written < samples_needed) {
        if (s_audio_block_avail <= 0) {
            int chunk = g_audio_frames_per_block;
            if (chunk > 1024) chunk = 1024;
            RtlRenderAudio(s_audio_block, chunk, AUDIO_CHANNELS);
            s_audio_block_avail = chunk;
            s_audio_block_pos = 0;
        }
        int to_copy = samples_needed - written;
        if (to_copy > s_audio_block_avail) {
            to_copy = s_audio_block_avail;
        }
        memcpy(out + written * AUDIO_CHANNELS,
               s_audio_block + s_audio_block_pos * AUDIO_CHANNELS,
               to_copy * AUDIO_CHANNELS * sizeof(int16_t));
        s_audio_block_pos += to_copy;
        s_audio_block_avail -= to_copy;
        written += to_copy;
    }

    int vol = g_issd_config.master_volume;
    if (vol < 100 && vol >= 0) {
        for (int i = 0; i < samples_needed * AUDIO_CHANNELS; i++) {
            out[i] = (int16_t)(((int32_t)out[i] * vol) / 100);
        }
    }
}

static void IssdDrawPpuFrame(void) {
    if (!g_snes || !g_snes->ppu) return;

    issd_widescreen_begin(g_snes->ppu, g_ram, g_rom_data, g_rom_size,
                         g_ws_active ? g_ws_extra : 0);

    SimpleHdma hdma[8];
    for (int ch = 0; ch < 8; ch++) {
        SimpleHdma_Init(&hdma[ch], &g_snes->dma->channel[ch]);
    }

    issd_hd_begin_frame();
    for (int line = 0; line <= SNES_HEIGHT; line++) {
        if (line > 0) {
            for (int ch = 0; ch < 8; ch++) {
                SimpleHdma_DoLine(&hdma[ch]);
            }
        }
        /* After this line's HDMA, before it is drawn: the registers a
         * replacement pass needs are the ones in force right now, and
         * the title screen changes background mode partway down. */
        issd_hd_note_line(g_snes->ppu, line);
        ppu_runLine(g_snes->ppu, line);
    }
    ppu_handleVblank(g_snes->ppu);
    issd_widescreen_end(g_snes->ppu);
    issd_hd_dump_frame(g_snes->ppu);
}

static void IssdInitialize(void) {
    printf("[ISSD Native] Initializing ISSD Bridge and CPU state...\n");
    cpu_state_init(&g_cpu, g_ram);
    issd_bridge_init();
}

static void IssdBootReset(void) {
    printf("[ISSD Native] Executing Reset Vector ($80:8000)...\n");
    static const uint32_t stop_pcs[] = { 0x8080D4, 0x0080D4 };
    int ok = interp_bridge_resume_task(&g_cpu, 0x808000, g_cpu.S, stop_pcs, 2);
    printf("[ISSD Native] Reset sequence completed (result: %d, PB: $%02X, S: $%04X, D: $%04X, DB: $%02X, Y: $%04X).\n",
           ok, (unsigned)g_cpu.PB, (unsigned)g_cpu.S, (unsigned)g_cpu.D, (unsigned)g_cpu.DB, (unsigned)g_cpu.Y);
    uint32_t ptr = (uint32_t)g_ram[(g_cpu.D + 0x15) & 0x1FFFF] |
                   ((uint32_t)g_ram[(g_cpu.D + 0x16) & 0x1FFFF] << 8) |
                   ((uint32_t)g_ram[(g_cpu.D + 0x17) & 0x1FFFF] << 16);
    printf("[ISSD Native] [D+$15] = $%06X\n", ptr);
    printf("[ISSD Native] Searching g_ram for C8:\n");
    for (int i = 0; i < 0x20000; i++) {
        if (g_ram[i] == 0xC8 && g_ram[i+1] == 0xF0) {
            printf("  contiguous C8 at g_ram[$%05X]: ", i);
            for (int j = 0; j < 16; j++) printf("%02X ", g_ram[i+j]);
            printf("\n");
        }
        if (g_ram[i] == 0xC8 && g_ram[i+2] == 0xF0) {
            printf("  interleaved C8 at g_ram[$%05X]: ", i);
            for (int j = 0; j < 16; j++) printf("%02X ", g_ram[i+j]);
            printf("\n");
        }
    }
    printf("[ISSD Native] CPU Stack @ $01A0..$01BF: ");
    for (int i = 0x01A0; i <= 0x01BF; i++) {
        printf("%02X ", g_cpu.ram[i]);
    }
    printf("\n");
    if (g_snes && g_snes->apu && g_snes->apu->spc) {
        printf("[ISSD Native] SPC PC: $%04X, in: [%02X %02X %02X %02X], out: [%02X %02X %02X %02X]\n",
               g_snes->apu->spc->pc,
               g_snes->apu->inPorts[0], g_snes->apu->inPorts[1], g_snes->apu->inPorts[2], g_snes->apu->inPorts[3],
               g_snes->apu->outPorts[0], g_snes->apu->outPorts[1], g_snes->apu->outPorts[2], g_snes->apu->outPorts[3]);
        printf("[ISSD Native] SPC RAM @ PC-4: ");
        for (int i = -4; i < 12; i++) {
            uint16_t a = (uint16_t)(g_snes->apu->spc->pc + i);
            printf("%02X ", g_snes->apu->ram[a]);
        }
        printf("\n");
        printf("[ISSD Native] SPC RAM $0200..$0350:\n");
        for (int a = 0x0200; a <= 0x0350; a += 16) {
            printf("  $%04X: ", a);
            for (int j = 0; j < 16; j++) printf("%02X ", g_snes->apu->ram[a + j]);
            printf("\n");
        }
        extern uint64_t g_spc_pc_histogram[0x10000];
        printf("[ISSD Native] SPC visited PCs:\n");
        for (int p = 0; p < 0x10000; p++) {
            if (g_spc_pc_histogram[p] > 0) {
                printf("  $%04X: %llu\n", p, (unsigned long long)g_spc_pc_histogram[p]);
            }
        }
    }
    fflush(stdout);
}

static void IssdRunFrame(void) {
    /* Push hardware interrupt frame (PB, PC_hi, PC_lo, P) for 65816 RTI compatibility */
    uint16_t return_pc = 0x80D4;
    cpu_mirrors_to_p(&g_cpu);
    cpu_write8(&g_cpu, 0x00, g_cpu.S--, g_cpu.PB);
    cpu_write8(&g_cpu, 0x00, g_cpu.S--, (return_pc >> 8) & 0xFF);
    cpu_write8(&g_cpu, 0x00, g_cpu.S--, return_pc & 0xFF);
    cpu_write8(&g_cpu, 0x00, g_cpu.S--, g_cpu.P);

    /* Apply Debug / Japanese developer cheat flags */
    if (g_issd_config.debug_unhooked_code) {
        g_ram[0x1D854] = 1; /* Dog referee cheat flag ($7ED854) */
        g_ram[0x1D855] = 0;
        g_ram[0x1D856] = 1; /* Unlock All-Star teams ($7ED856) */
        g_ram[0x1D857] = 0;
        g_ram[0x1D858] = 1; /* Superstar difficulty / stats ($7ED858) */
        g_ram[0x1D859] = 0;
    }

    /* Execute 1 frame via NMI interrupt handler at $80:80E0 */
    interp_tier_dispatch_interrupt(&g_cpu, 0x8080E0);

    /* Frame health sanitization: If NMI bailed mid-flight, sanitize $3C so subsequent frames can run */
    if (g_ram[0x3c] != 0) {
        g_ram[0x3c] = 0;
        g_ram[0x3d] = 0;
    }
    if (g_cpu.S < 0x01A0 || g_cpu.S > 0x01AF) {
        g_cpu.S = 0x01AF;
    }
}

static const RtlGameInfo kIssdGameInfo = {
    .initialize = IssdInitialize,
    .run_frame = IssdRunFrame,
    .draw_ppu_frame = IssdDrawPpuFrame,
    .save_name_prefix = "issd_save",
};

static uint8_t *LoadRomFile(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (size_t)size;
    return data;
}

/* The generated bodies for the three SPC700 handshake routines desynchronise
 * the IPL port echo, wedging the reset vector before a window is ever created.
 * The runner can tier them down to the interpreter, but only if it is pointed
 * at a deny set. Apply the shipped one by default so a clean checkout boots;
 * an explicit SNESRECOMP_LLE_INTERP_TARGET_FILE still wins, which is what
 * bisecting the underlying codegen bug needs. */
#define AOT_BOOT_DENY_NAME "aot_boot_deny.txt"

static bool FileExists(const char *path);

static void ApplyDefaultAotBootDenySet(const char *argv0) {
    const char *existing = getenv("SNESRECOMP_LLE_INTERP_TARGET_FILE");
    if (existing && existing[0]) return;

    char candidate[1024];
    const char *found = NULL;

    snprintf(candidate, sizeof(candidate), "recomp/" AOT_BOOT_DENY_NAME);
    if (FileExists(candidate)) found = candidate;

#ifdef _WIN32
    char exe_path[1024];
    if (!found && GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) {
#else
    char exe_path[1024];
    snprintf(exe_path, sizeof(exe_path), "%s", argv0 ? argv0 : "");
    if (!found && exe_path[0]) {
#endif
        char *slash = strrchr(exe_path, '\\');
        char *fwd = strrchr(exe_path, '/');
        if (!slash || (fwd && fwd > slash)) slash = fwd;
        if (slash) {
            *slash = '\0';
            snprintf(candidate, sizeof(candidate), "%s/" AOT_BOOT_DENY_NAME, exe_path);
            if (FileExists(candidate)) found = candidate;
            if (!found) {
                snprintf(candidate, sizeof(candidate),
                         "%s/../recomp/" AOT_BOOT_DENY_NAME, exe_path);
                if (FileExists(candidate)) found = candidate;
            }
            if (!found) {
                snprintf(candidate, sizeof(candidate),
                         "%s/../../recomp/" AOT_BOOT_DENY_NAME, exe_path);
                if (FileExists(candidate)) found = candidate;
            }
        }
    }
    (void)argv0;

    if (!found) {
        fprintf(stderr, "[Boot] WARNING: %s not found; the SPC700 handshake may "
                        "wedge the reset vector and no window will appear.\n",
                AOT_BOOT_DENY_NAME);
        return;
    }
#ifdef _WIN32
    _putenv_s("SNESRECOMP_LLE_INTERP_TARGET_FILE", found);
#else
    setenv("SNESRECOMP_LLE_INTERP_TARGET_FILE", found, 0);
#endif
    printf("[Boot] AOT deny set: %s\n", found);
}

static bool FileExists(const char *path) {
    if (!path || !path[0]) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void BuildDefaultConfigPath(char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
#ifdef ISSD_ANDROID
    /* App-private external storage: no runtime permission needed, and the
     * user can still reach it over USB to drop in a ROM. */
    if (issd_android_external_dir()[0]) {
        snprintf(out, out_size, "%s/issd_native.cfg", issd_android_external_dir());
        return;
    }
#endif
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata && appdata[0]) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s\\ISSDNative", appdata);
        CreateDirectoryA(dir, NULL);
        snprintf(out, out_size, "%s\\issd_native.cfg", dir);
        return;
    }
#else
    /* XDG Base Directory: $XDG_CONFIG_HOME, else ~/.config. This is what
     * SteamOS expects, and it keeps config off a read-only install path. */
    {
        char dir[1024];
        const char *xdg = getenv("XDG_CONFIG_HOME");
        const char *home = getenv("HOME");
        dir[0] = 0;
        if (xdg && xdg[0])
            snprintf(dir, sizeof(dir), "%s/ISSDNative", xdg);
        else if (home && home[0])
            snprintf(dir, sizeof(dir), "%s/.config/ISSDNative", home);
        if (dir[0]) {
            char parent[1024];
            snprintf(parent, sizeof(parent), "%s",
                     (xdg && xdg[0]) ? xdg : home);
            if (!(xdg && xdg[0])) {
                snprintf(parent, sizeof(parent), "%s/.config", home);
                mkdir(parent, 0755);
            }
            if (mkdir(dir, 0755) == 0 || errno == EEXIST) {
                snprintf(out, out_size, "%s/issd_native.cfg", dir);
                return;
            }
        }
    }
#endif
    snprintf(out, out_size, "issd_native.cfg");
}

static const char *ResolveConfigPath(char *out, size_t out_size, const char *cli_path) {
    if (cli_path && cli_path[0]) {
        snprintf(out, out_size, "%s", cli_path);
        return out;
    }
    if (FileExists("issd_native.cfg")) {
        snprintf(out, out_size, "%s", "issd_native.cfg");
        return out;
    }
    if (FileExists("issd_config.json")) {
        snprintf(out, out_size, "%s", "issd_config.json");
        return out;
    }
    BuildDefaultConfigPath(out, out_size);
    return out;
}

/* The runner already ships a cross-platform picker: a Win32 dialog on Windows,
 * zenity or kdialog on Linux, and a clear diagnostic when neither is installed.
 * Using it replaces a Windows-only copy of the same dialog and is what gives
 * the SteamOS build a working ROM chooser. */
static bool PromptForRomFile(char *out, size_t out_size) {
    if (!out || out_size == 0) return false;
    out[0] = '\0';
#ifdef ISSD_ANDROID
    /* Android has no file dialog callable from C. Scan the app's own
     * external directory, which is where a user can copy a cartridge
     * image over USB without granting storage permissions. */
    return issd_android_find_rom(out, out_size);
#else
    return snesrecomp_pick_rom_file(out, out_size) == 1 && out[0] != 0;
#endif
}

static SDL_GameController *g_controller = NULL;
static SDL_Window *g_window = NULL;

/* SDL reports touches normalised to the window, and reports each finger
 * independently. issd_touch wants the full set of live points in window
 * pixels once per frame, so collect them from SDL's own finger state
 * rather than trying to track down/up transitions ourselves. */
static void PumpTouchState(void) {
    if (!g_window || !issd_touch_enabled()) return;
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(g_window, &win_w, &win_h);
    issd_touch_set_viewport(win_w, win_h);

    int xs[10], ys[10], n = 0;
    const int devices = SDL_GetNumTouchDevices();
    for (int d = 0; d < devices && n < 10; d++) {
        const SDL_TouchID id = SDL_GetTouchDevice(d);
        const int fingers = SDL_GetNumTouchFingers(id);
        for (int f = 0; f < fingers && n < 10; f++) {
            SDL_Finger *finger = SDL_GetTouchFinger(id, f);
            if (!finger) continue;
            xs[n] = (int)(finger->x * (float)win_w);
            ys[n] = (int)(finger->y * (float)win_h);
            n++;
        }
    }
    issd_touch_set_points(xs, ys, n);

    if (issd_touch_take_menu_press()) issd_menu_toggle();
}

static void TouchDrawFilledCircle(SDL_Renderer *renderer, int cx, int cy, int radius,
                                 Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(r2 - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void TouchDrawCircleOutline(SDL_Renderer *renderer, int cx, int cy, int radius, int thickness,
                                  Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    for (int t = 0; t < thickness; t++) {
        int rad = radius - t;
        if (rad < 0) break;
        int x = rad, y = 0;
        int err = 0;
        while (x >= y) {
            SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            SDL_RenderDrawPoint(renderer, cx + y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - x, cy + y);
            SDL_RenderDrawPoint(renderer, cx - x, cy - y);
            SDL_RenderDrawPoint(renderer, cx - y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + x, cy - y);
            if (err <= 0) {
                y += 1;
                err += 2 * y + 1;
            }
            if (err > 0) {
                x -= 1;
                err -= 2 * x + 1;
            }
        }
    }
}

static void TouchDrawRoundedRect(SDL_Renderer *renderer, const SDL_Rect *rect, int radius,
                                Uint8 r, Uint8 g, Uint8 b, Uint8 a, bool filled) {
    if (radius * 2 > rect->h) radius = rect->h / 2;
    if (radius * 2 > rect->w) radius = rect->w / 2;
    if (radius < 1) radius = 1;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);

    if (filled) {
        /* Center block spanning full height */
        SDL_Rect mid = { rect->x + radius, rect->y, rect->w - 2 * radius, rect->h };
        SDL_RenderFillRect(renderer, &mid);

        /* Left and right side blocks between the corner arcs */
        if (rect->h > 2 * radius) {
            SDL_Rect left_side  = { rect->x, rect->y + radius, radius, rect->h - 2 * radius };
            SDL_Rect right_side = { rect->x + rect->w - radius, rect->y + radius, radius, rect->h - 2 * radius };
            SDL_RenderFillRect(renderer, &left_side);
            SDL_RenderFillRect(renderer, &right_side);
        }

        /* 4 corner arcs */
        int r2 = radius * radius;
        for (int dy = 0; dy < radius; dy++) {
            int dx = (int)sqrtf((float)(r2 - dy * dy));
            int top_y = rect->y + radius - 1 - dy;
            int bot_y = rect->y + rect->h - radius + dy;
            /* Top-left */
            SDL_RenderDrawLine(renderer, rect->x + radius - dx, top_y, rect->x + radius, top_y);
            /* Top-right */
            SDL_RenderDrawLine(renderer, rect->x + rect->w - radius, top_y, rect->x + rect->w - radius + dx, top_y);
            /* Bottom-left */
            SDL_RenderDrawLine(renderer, rect->x + radius - dx, bot_y, rect->x + radius, bot_y);
            /* Bottom-right */
            SDL_RenderDrawLine(renderer, rect->x + rect->w - radius, bot_y, rect->x + rect->w - radius + dx, bot_y);
        }
    } else {
        /* Outline: 4 straight edges */
        SDL_RenderDrawLine(renderer, rect->x + radius, rect->y, rect->x + rect->w - radius, rect->y);
        SDL_RenderDrawLine(renderer, rect->x + radius, rect->y + rect->h - 1, rect->x + rect->w - radius, rect->y + rect->h - 1);
        if (rect->h > 2 * radius) {
            SDL_RenderDrawLine(renderer, rect->x, rect->y + radius, rect->x, rect->y + rect->h - radius);
            SDL_RenderDrawLine(renderer, rect->x + rect->w - 1, rect->y + radius, rect->x + rect->w - 1, rect->y + rect->h - radius);
        }

        /* 4 corner arc outlines */
        int x = radius, y = 0;
        int err = 0;
        int cx_l = rect->x + radius;
        int cx_r = rect->x + rect->w - radius - 1;
        int cy_t = rect->y + radius;
        int cy_b = rect->y + rect->h - radius - 1;
        while (x >= y) {
            /* Top-left */
            SDL_RenderDrawPoint(renderer, cx_l - x, cy_t - y);
            SDL_RenderDrawPoint(renderer, cx_l - y, cy_t - x);
            /* Top-right */
            SDL_RenderDrawPoint(renderer, cx_r + x, cy_t - y);
            SDL_RenderDrawPoint(renderer, cx_r + y, cy_t - x);
            /* Bottom-left */
            SDL_RenderDrawPoint(renderer, cx_l - x, cy_b + y);
            SDL_RenderDrawPoint(renderer, cx_l - y, cy_b + x);
            /* Bottom-right */
            SDL_RenderDrawPoint(renderer, cx_r + x, cy_b + y);
            SDL_RenderDrawPoint(renderer, cx_r + y, cy_b + x);
            if (err <= 0) {
                y += 1;
                err += 2 * y + 1;
            }
            if (err > 0) {
                x -= 1;
                err -= 2 * x + 1;
            }
        }
    }
}

static void TouchDrawText(SDL_Renderer *renderer, const char *text, int cx, int cy, int scale,
                         Uint8 r, Uint8 g, Uint8 b, Uint8 a, bool shadow) {
    if (!text || !*text) return;
    int len = (int)strlen(text);
    int total_w = len * 8 * scale;
    int total_h = 8 * scale;
    int start_x = cx - total_w / 2;
    int start_y = cy - total_h / 2;

    if (shadow) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(a * 0.85f));
        int off = scale > 1 ? scale : 1;
        for (int i = 0; i < len; i++) {
            char ch = text[i];
            if (ch < 32 || ch > 126) ch = ' ';
            const uint8_t *glyph = g_issd_font8x8[ch - 32];
            int gx = start_x + i * 8 * scale + off;
            int gy = start_y + off;
            for (int row = 0; row < 8; row++) {
                uint8_t bits = glyph[row];
                if (!bits) continue;
                for (int col = 0; col < 8; col++) {
                    if (bits & (0x80 >> col)) {
                        SDL_Rect px_box = { gx + col * scale, gy + row * scale, scale, scale };
                        SDL_RenderFillRect(renderer, &px_box);
                    }
                }
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    for (int i = 0; i < len; i++) {
        char ch = text[i];
        if (ch < 32 || ch > 126) ch = ' ';
        const uint8_t *glyph = g_issd_font8x8[ch - 32];
        int gx = start_x + i * 8 * scale;
        int gy = start_y;
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            if (!bits) continue;
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    SDL_Rect px_box = { gx + col * scale, gy + row * scale, scale, scale };
                    SDL_RenderFillRect(renderer, &px_box);
                }
            }
        }
    }
}

static void TouchDrawTriangle(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, int x2, int y2,
                             Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    int min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int max_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int y = min_y; y <= max_y; y++) {
        int x_coords[3];
        int num = 0;
        int pts[3][2] = { {x0, y0}, {x1, y1}, {x2, y2} };
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            int py0 = pts[i][1], py1 = pts[j][1];
            int px0 = pts[i][0], px1 = pts[j][0];
            if ((py0 <= y && y < py1) || (py1 <= y && y < py0)) {
                x_coords[num++] = px0 + (y - py0) * (px1 - px0) / (py1 - py0);
            }
        }
        if (num == 2) {
            int start = x_coords[0] < x_coords[1] ? x_coords[0] : x_coords[1];
            int end   = x_coords[0] > x_coords[1] ? x_coords[0] : x_coords[1];
            SDL_RenderDrawLine(renderer, start, y, end, y);
        }
    }
}

/* Drawn straight onto the renderer after the game frame, so the overlay is
 * always at native window resolution rather than the 256-pixel-wide SNES
 * buffer, and never gets scaled into mush. */
static void RenderTouchOverlay(SDL_Renderer *renderer) {
    if (!renderer || !issd_touch_enabled()) return;
    const IssdTouchRect *rects = NULL;
    const int count = issd_touch_rects(&rects);
    if (!rects) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    uint16_t pad_mask = issd_touch_pad_mask();

    for (int i = 0; i < count; i++) {
        const IssdTouchRect *r = &rects[i];
        if (!r->visible) continue;

        if (i == ISSD_TOUCH_DPAD) {
            /* Authentic SNES Cross D-Pad */
            const int aw = r->w / 3;
            const int ah = r->h / 3;
            const int cx = r->x + r->w / 2;
            const int cy = r->y + r->h / 2;

            /* Base cross shapes */
            SDL_Rect h_bar = { r->x, r->y + ah, r->w, ah };
            SDL_Rect v_bar = { r->x + aw, r->y, aw, r->h };

            SDL_SetRenderDrawColor(renderer, 30, 35, 45, 180);
            SDL_RenderFillRect(renderer, &h_bar);
            SDL_RenderFillRect(renderer, &v_bar);

            /* Directional active arms / glow */
            if (pad_mask & ISSD_PAD_UP) {
                SDL_Rect up_bar = { r->x + aw, r->y, aw, ah };
                SDL_SetRenderDrawColor(renderer, 80, 160, 240, 220);
                SDL_RenderFillRect(renderer, &up_bar);
            }
            if (pad_mask & ISSD_PAD_DOWN) {
                SDL_Rect dn_bar = { r->x + aw, r->y + 2 * ah, aw, ah };
                SDL_SetRenderDrawColor(renderer, 80, 160, 240, 220);
                SDL_RenderFillRect(renderer, &dn_bar);
            }
            if (pad_mask & ISSD_PAD_LEFT) {
                SDL_Rect lf_bar = { r->x, r->y + ah, aw, ah };
                SDL_SetRenderDrawColor(renderer, 80, 160, 240, 220);
                SDL_RenderFillRect(renderer, &lf_bar);
            }
            if (pad_mask & ISSD_PAD_RIGHT) {
                SDL_Rect rt_bar = { r->x + 2 * aw, r->y + ah, aw, ah };
                SDL_SetRenderDrawColor(renderer, 80, 160, 240, 220);
                SDL_RenderFillRect(renderer, &rt_bar);
            }

            /* Cross outline */
            SDL_SetRenderDrawColor(renderer, 100, 110, 130, 220);
            /* Top edge */
            SDL_RenderDrawLine(renderer, r->x + aw, r->y, r->x + 2 * aw, r->y);
            /* Top-right notch */
            SDL_RenderDrawLine(renderer, r->x + 2 * aw, r->y, r->x + 2 * aw, r->y + ah);
            SDL_RenderDrawLine(renderer, r->x + 2 * aw, r->y + ah, r->x + r->w, r->y + ah);
            /* Right edge */
            SDL_RenderDrawLine(renderer, r->x + r->w, r->y + ah, r->x + r->w, r->y + 2 * ah);
            /* Bottom-right notch */
            SDL_RenderDrawLine(renderer, r->x + r->w, r->y + 2 * ah, r->x + 2 * aw, r->y + 2 * ah);
            SDL_RenderDrawLine(renderer, r->x + 2 * aw, r->y + 2 * ah, r->x + 2 * aw, r->y + r->h);
            /* Bottom edge */
            SDL_RenderDrawLine(renderer, r->x + 2 * aw, r->y + r->h, r->x + aw, r->y + r->h);
            /* Bottom-left notch */
            SDL_RenderDrawLine(renderer, r->x + aw, r->y + r->h, r->x + aw, r->y + 2 * ah);
            SDL_RenderDrawLine(renderer, r->x + aw, r->y + 2 * ah, r->x, r->y + 2 * ah);
            /* Left edge */
            SDL_RenderDrawLine(renderer, r->x, r->y + 2 * ah, r->x, r->y + ah);
            /* Top-left notch */
            SDL_RenderDrawLine(renderer, r->x, r->y + ah, r->x + aw, r->y + ah);
            SDL_RenderDrawLine(renderer, r->x + aw, r->y + ah, r->x + aw, r->y);

            /* Center pivot dimple */
            TouchDrawFilledCircle(renderer, cx, cy, aw / 4, 20, 25, 30, 220);
            TouchDrawCircleOutline(renderer, cx, cy, aw / 4, 1, 70, 80, 100, 200);

            /* Directional chevrons / arrows */
            const int tri_s = aw / 4;
            /* UP */
            TouchDrawTriangle(renderer, cx, (int)(r->y + ah * 0.3f),
                              cx - tri_s, (int)(r->y + ah * 0.75f),
                              cx + tri_s, (int)(r->y + ah * 0.75f),
                              255, 255, 255, (pad_mask & ISSD_PAD_UP) ? 255 : 180);
            /* DOWN */
            TouchDrawTriangle(renderer, cx, (int)(r->y + r->h - ah * 0.3f),
                              cx - tri_s, (int)(r->y + r->h - ah * 0.75f),
                              cx + tri_s, (int)(r->y + r->h - ah * 0.75f),
                              255, 255, 255, (pad_mask & ISSD_PAD_DOWN) ? 255 : 180);
            /* LEFT */
            TouchDrawTriangle(renderer, (int)(r->x + aw * 0.3f), cy,
                              (int)(r->x + aw * 0.75f), cy - tri_s,
                              (int)(r->x + aw * 0.75f), cy + tri_s,
                              255, 255, 255, (pad_mask & ISSD_PAD_LEFT) ? 255 : 180);
            /* RIGHT */
            TouchDrawTriangle(renderer, (int)(r->x + r->w - aw * 0.3f), cy,
                              (int)(r->x + r->w - aw * 0.75f), cy - tri_s,
                              (int)(r->x + r->w - aw * 0.75f), cy + tri_s,
                              255, 255, 255, (pad_mask & ISSD_PAD_RIGHT) ? 255 : 180);

        } else if (r->round) {
            /* Circular face button with authentic SNES coloring, letter, and action sublabel */
            const int cx = r->x + r->w / 2;
            const int cy = r->y + r->h / 2;
            const int rad = r->w / 2;

            /* Base fill (translucent to see field beneath, bright when pressed) */
            const Uint8 fill_alpha = r->pressed ? 240 : 155;
            TouchDrawFilledCircle(renderer, cx, cy, rad, r->color_r, r->color_g, r->color_b, fill_alpha);

            /* Outer glow / outline */
            TouchDrawCircleOutline(renderer, cx, cy, rad, 3,
                                   r->pressed ? 255 : r->color_r,
                                   r->pressed ? 255 : r->color_g,
                                   r->pressed ? 255 : r->color_b,
                                   r->pressed ? 255 : 220);

            /* Subtle top specular arc highlight */
            TouchDrawCircleOutline(renderer, cx, cy - 2, rad - 5, 2, 255, 255, 255, r->pressed ? 120 : 60);

            /* Main button letter (A, B, X, Y) */
            int scale = rad / 14;
            if (scale < 2) scale = 2;
            int text_y = r->sublabel ? (cy - (int)(rad * 0.28f)) : cy;
            TouchDrawText(renderer, r->label, cx, text_y, scale, 255, 255, 255, 255, true);

            /* Action sublabel ("SHOOT", "PASS", "DASH", "THRU") */
            if (r->sublabel) {
                int sub_scale = rad / 28;
                if (sub_scale < 1) sub_scale = 1;
                TouchDrawText(renderer, r->sublabel, cx, cy + (int)(rad * 0.32f), sub_scale,
                              255, 255, 210, 240, true);
            }

        } else {
            /* Pill / capsule button for shoulders (L/R), START/SELECT, HIDE/SHOW, MENU */
            SDL_Rect box = { r->x, r->y, r->w, r->h };
            const int pill_rad = r->h / 2;
            const Uint8 fill_alpha = r->pressed ? 240 : 160;

            TouchDrawRoundedRect(renderer, &box, pill_rad,
                                 r->color_r, r->color_g, r->color_b, fill_alpha, true);
            TouchDrawRoundedRect(renderer, &box, pill_rad,
                                 r->pressed ? 255 : (Uint8)((r->color_r + 255) / 2),
                                 r->pressed ? 255 : (Uint8)((r->color_g + 255) / 2),
                                 r->pressed ? 255 : (Uint8)((r->color_b + 255) / 2),
                                 r->pressed ? 255 : 210, false);

            int scale = r->h / 22;
            if (scale < 1) scale = 1;
            TouchDrawText(renderer, r->label, r->x + r->w / 2, r->y + r->h / 2, scale,
                          255, 255, 255, 255, true);
        }
    }
}

static void ToggleFullscreen(void) {
    if (!g_window) return;
    g_issd_config.fullscreen = !g_issd_config.fullscreen;
    SDL_SetWindowFullscreen(g_window, g_issd_config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    printf("[Video] Fullscreen %s\n", g_issd_config.fullscreen ? "ENABLED" : "DISABLED");
}

static void ProcessInputEvent(const SDL_Event *ev) {
    if (ev->type == SDL_CONTROLLERDEVICEADDED) {
        if (!g_controller) {
            g_controller = SDL_GameControllerOpen(ev->cdevice.which);
            if (g_controller) {
                printf("[Input] GameController connected: %s\n", SDL_GameControllerName(g_controller));
            }
        }
    } else if (ev->type == SDL_CONTROLLERDEVICEREMOVED) {
        if (g_controller) {
            SDL_GameControllerClose(g_controller);
            g_controller = NULL;
            printf("[Input] GameController disconnected.\n");
        }
    } else if (ev->type == SDL_CONTROLLERBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONUP) {
        bool down = (ev->type == SDL_CONTROLLERBUTTONDOWN);
        Uint8 btn = ev->cbutton.button;

        /* Check for In-Game Menu Toggle via Gamepad Guide / Home or Back+Start */
        if (down) {
            bool is_guide = (btn == SDL_CONTROLLER_BUTTON_GUIDE);
            bool is_back_start = false;
            if (g_controller) {
                if (btn == SDL_CONTROLLER_BUTTON_START &&
                    SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_BACK)) {
                    is_back_start = true;
                } else if (btn == SDL_CONTROLLER_BUTTON_BACK &&
                           SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START)) {
                    is_back_start = true;
                }
            }
            if (is_guide || is_back_start) {
                issd_menu_toggle();
                return;
            }
        }

        if (issd_menu_is_open()) {
            if (down) {
                if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) issd_menu_navigate_up();
                else if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) issd_menu_navigate_down();
                else if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) issd_menu_navigate_left();
                else if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) issd_menu_navigate_right();
                else if (btn == SDL_CONTROLLER_BUTTON_A) issd_menu_confirm();
                else if (btn == SDL_CONTROLLER_BUTTON_B || btn == SDL_CONTROLLER_BUTTON_BACK) issd_menu_cancel();
            }
            return;
        }

        /* Gameplay Controller Decoding according to active Schema */
        IssdControlSchema schema = g_overlay_menu.control_schema;

        if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) {
            if (down) g_pad1_state |= (1 << 4); else g_pad1_state &= ~(1 << 4);
        } else if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
            if (down) g_pad1_state |= (1 << 5); else g_pad1_state &= ~(1 << 5);
        } else if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
            if (down) g_pad1_state |= (1 << 6); else g_pad1_state &= ~(1 << 6);
        } else if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            if (down) g_pad1_state |= (1 << 7); else g_pad1_state &= ~(1 << 7);
        } else if (btn == SDL_CONTROLLER_BUTTON_START) {
            if (down) g_pad1_state |= (1 << 3); else g_pad1_state &= ~(1 << 3); /* Start / Pause */
        } else if (btn == SDL_CONTROLLER_BUTTON_BACK) {
            if (down) g_pad1_state |= (1 << 2); else g_pad1_state &= ~(1 << 2); /* Select */
        } else if (schema == ISSD_SCHEMA_FIFA) {
            /* FIFA / EA FC Layout:
               A (Bottom) = Ground Pass (SNES B: 1<<0)
               B (Right)  = Shoot (SNES A: 1<<8)
               X (Left)   = Long Pass / Cross (SNES Y: 1<<1)
               Y (Top)    = Through Ball (SNES X: 1<<9)
               RB         = Sprint Dash (SNES Y: 1<<1)
               LB         = Player Switch / Tactics (SNES L: 1<<10)
            */
            if (btn == SDL_CONTROLLER_BUTTON_A) {
                if (down) g_pad1_state |= (1 << 0); else g_pad1_state &= ~(1 << 0);
            } else if (btn == SDL_CONTROLLER_BUTTON_B) {
                if (down) g_pad1_state |= (1 << 8); else g_pad1_state &= ~(1 << 8);
            } else if (btn == SDL_CONTROLLER_BUTTON_X) {
                if (down) g_pad1_state |= (1 << 1); else g_pad1_state &= ~(1 << 1);
            } else if (btn == SDL_CONTROLLER_BUTTON_Y) {
                if (down) g_pad1_state |= (1 << 9); else g_pad1_state &= ~(1 << 9);
            } else if (btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                if (down) g_pad1_state |= (1 << 1); else g_pad1_state &= ~(1 << 1);
            } else if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                if (down) g_pad1_state |= (1 << 10); else g_pad1_state &= ~(1 << 10);
            }
        } else if (schema == ISSD_SCHEMA_PES) {
            /* PES / eFootball Layout:
               A (Bottom) = Ground Pass (SNES B: 1<<0)
               X (Left)   = Shoot (SNES A: 1<<8)
               B (Right)  = Long Pass / Cross (SNES Y: 1<<1)
               Y (Top)    = Through Ball (SNES X: 1<<9)
               RB         = Sprint Dash (SNES Y: 1<<1)
               LB         = Cursor Change (SNES L: 1<<10)
            */
            if (btn == SDL_CONTROLLER_BUTTON_A) {
                if (down) g_pad1_state |= (1 << 0); else g_pad1_state &= ~(1 << 0);
            } else if (btn == SDL_CONTROLLER_BUTTON_X) {
                if (down) g_pad1_state |= (1 << 8); else g_pad1_state &= ~(1 << 8);
            } else if (btn == SDL_CONTROLLER_BUTTON_B) {
                if (down) g_pad1_state |= (1 << 1); else g_pad1_state &= ~(1 << 1);
            } else if (btn == SDL_CONTROLLER_BUTTON_Y) {
                if (down) g_pad1_state |= (1 << 9); else g_pad1_state &= ~(1 << 9);
            } else if (btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                if (down) g_pad1_state |= (1 << 1); else g_pad1_state &= ~(1 << 1);
            } else if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                if (down) g_pad1_state |= (1 << 10); else g_pad1_state &= ~(1 << 10);
            }
        } else {
            /* Classic ISSD / SNES Layout */
            if (btn == SDL_CONTROLLER_BUTTON_A) {
                if (down) g_pad1_state |= (1 << 0); else g_pad1_state &= ~(1 << 0);
            } else if (btn == SDL_CONTROLLER_BUTTON_B) {
                if (down) g_pad1_state |= (1 << 8); else g_pad1_state &= ~(1 << 8);
            } else if (btn == SDL_CONTROLLER_BUTTON_X) {
                if (down) g_pad1_state |= (1 << 1); else g_pad1_state &= ~(1 << 1);
            } else if (btn == SDL_CONTROLLER_BUTTON_Y) {
                if (down) g_pad1_state |= (1 << 9); else g_pad1_state &= ~(1 << 9);
            } else if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                if (down) g_pad1_state |= (1 << 10); else g_pad1_state &= ~(1 << 10);
            } else if (btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                if (down) g_pad1_state |= (1 << 11); else g_pad1_state &= ~(1 << 11);
            }
        }
    } else if (ev->type == SDL_CONTROLLERAXISMOTION) {
        int16_t val = ev->caxis.value;
        const int16_t deadzone = 12000;
        if (ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            if (val < -deadzone) {
                g_pad1_state |= (1 << 6);
                g_pad1_state &= ~(1 << 7);
            } else if (val > deadzone) {
                g_pad1_state |= (1 << 7);
                g_pad1_state &= ~(1 << 6);
            } else {
                g_pad1_state &= ~((1 << 6) | (1 << 7));
            }
        } else if (ev->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            if (val < -deadzone) {
                g_pad1_state |= (1 << 4);
                g_pad1_state &= ~(1 << 5);
            } else if (val > deadzone) {
                g_pad1_state |= (1 << 5);
                g_pad1_state &= ~(1 << 4);
            } else {
                g_pad1_state &= ~((1 << 4) | (1 << 5));
            }
        } else if (ev->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            /* Right Trigger -> Sprint Dash in FIFA/PES modes */
            if (val > deadzone) g_pad1_state |= (1 << 1);
            else g_pad1_state &= ~(1 << 1);
        } else if (ev->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
            /* Left Trigger -> Strategy Shift */
            if (val > deadzone) g_pad1_state |= (1 << 11);
            else g_pad1_state &= ~(1 << 11);
        }
    } else if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP) {
        bool down = (ev->type == SDL_KEYDOWN);
        SDL_Scancode code = ev->key.keysym.scancode;

        /* Check for In-Game Menu Toggle via Escape or F1 */
        if (down && (code == SDL_SCANCODE_ESCAPE || code == SDL_SCANCODE_F1)) {
            issd_menu_toggle();
            return;
        }

        /* When Menu Overlay is Open, Route Keyboard to Menu Navigation */
        if (issd_menu_is_open()) {
            if (down) {
                if (code == SDL_SCANCODE_UP || code == SDL_SCANCODE_W) issd_menu_navigate_up();
                else if (code == SDL_SCANCODE_DOWN || code == SDL_SCANCODE_S) issd_menu_navigate_down();
                else if (code == SDL_SCANCODE_LEFT || code == SDL_SCANCODE_A) issd_menu_navigate_left();
                else if (code == SDL_SCANCODE_RIGHT || code == SDL_SCANCODE_D) issd_menu_navigate_right();
                else if (code == SDL_SCANCODE_RETURN || code == SDL_SCANCODE_SPACE || code == SDL_SCANCODE_X) issd_menu_confirm();
                else if (code == SDL_SCANCODE_Z || code == SDL_SCANCODE_BACKSPACE) issd_menu_cancel();
            }
            return;
        }

        if (down) {
            if (code == SDL_SCANCODE_F2) {
                /* Cycle Control Schema Hotkey */
                g_overlay_menu.control_schema = (IssdControlSchema)((g_overlay_menu.control_schema + 1) % 3);
                const char *s_name = (g_overlay_menu.control_schema == ISSD_SCHEMA_CLASSIC) ? "CLASSIC ISSD" :
                                     (g_overlay_menu.control_schema == ISSD_SCHEMA_FIFA) ? "MODERN FIFA" : "MODERN PES";
                printf("[Input] Switched Control Schema to: %s\n", s_name);
                return;
            } else if (code == SDL_SCANCODE_F3) {
                /* Cycle Aspect Ratio */
                g_issd_config.aspect_ratio = (IssdAspectRatio)((g_issd_config.aspect_ratio + 1) % 6);
                const char *a_name = (g_issd_config.aspect_ratio == ISSD_ASPECT_4_3) ? "4:3 CRT" :
                                     (g_issd_config.aspect_ratio == ISSD_ASPECT_8_7) ? "8:7 PIXEL" :
                                     (g_issd_config.aspect_ratio == ISSD_ASPECT_16_9) ? "16:9 WIDE" :
                                     (g_issd_config.aspect_ratio == ISSD_ASPECT_16_10) ? "16:10 PC" :
                                     (g_issd_config.aspect_ratio == ISSD_ASPECT_21_9) ? "21:9 ULTRA" : "INTEGER SCALE";
                printf("[Video] Aspect Ratio set to: %s\n", a_name);
                return;
            } else if (code == SDL_SCANCODE_F4) {
                /* Cycle Internal Resolution (1x up to 8x 4K) */
                if (!issd_menu_internal_res_applies()) {
                    printf("[Video] Internal Resolution applies to the CRT filter only "
                           "(F8 to select it); Nearest and Linear scale the logical "
                           "buffer directly.\n");
                    return;
                }
                g_issd_config.internal_res = (IssdInternalResolution)((g_issd_config.internal_res + 1) % 6);
                const char *r_name = (g_issd_config.internal_res == ISSD_RES_1X) ? "1X (256x224)" :
                                     (g_issd_config.internal_res == ISSD_RES_2X) ? "2X (512x448)" :
                                     (g_issd_config.internal_res == ISSD_RES_3X) ? "3X (720p HD)" :
                                     (g_issd_config.internal_res == ISSD_RES_4X) ? "4X (1080p FHD)" :
                                     (g_issd_config.internal_res == ISSD_RES_6X) ? "6X (1440p QHD)" : "8X (4K UHD)";
                printf("[Video] Internal Resolution set to: %s\n", r_name);
                return;
            } else if (code == SDL_SCANCODE_F5) {
                issd_save_quick();
                return;
            } else if (code == SDL_SCANCODE_F6) {
                issd_load_quick();
                return;
            } else if (code == SDL_SCANCODE_F7) {
                /* Cycle Target FPS */
                static const int s_fps[] = { 60, 120, 144, 165, 240, 0 };
                int idx = 0;
                for (int i = 0; i < 6; i++) {
                    if (g_issd_config.target_fps == s_fps[i]) { idx = i; break; }
                }
                idx = (idx + 1) % 6;
                g_issd_config.target_fps = s_fps[idx];
                printf("[Video] Target FPS set to: %d Hz (0=uncapped)\n", g_issd_config.target_fps);
                return;
            } else if (code == SDL_SCANCODE_F8) {
                /* Cycle Scaling Filter */
                g_issd_config.scaling_filter = (IssdScalingFilter)((g_issd_config.scaling_filter + 1) % 3);
                const char *f_name = (g_issd_config.scaling_filter == ISSD_FILTER_NEAREST) ? "NEAREST (SHARP)" :
                                     (g_issd_config.scaling_filter == ISSD_FILTER_LINEAR) ? "LINEAR (SMOOTH)" : "CRT SCANLINES";
                printf("[Video] Scaling Filter set to: %s\n", f_name);
                return;
            } else if (code == SDL_SCANCODE_F11) {
                ToggleFullscreen();
                return;
            } else if (code == SDL_SCANCODE_TAB) {
                static bool s_ff = false;
                s_ff = !s_ff;
                RtlAudioSetFastForward(s_ff);
                printf("[Input] Fast-Forward %s\n", s_ff ? "ENABLED" : "DISABLED");
                return;
            } else if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_8) {
                int slot = code - SDL_SCANCODE_1;
                const Uint8 *keys = SDL_GetKeyboardState(NULL);
                if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) {
                    issd_save_to_slot(slot, NULL);
                } else if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) {
                    issd_load_from_slot(slot);
                }
                return;
            }
        }

        switch (code) {
            case SDL_SCANCODE_Z:
            case SDL_SCANCODE_J:
                if (down) g_pad1_state |= (1 << 0); else g_pad1_state &= ~(1 << 0); break; /* B (Short pass / Cancel) */
            case SDL_SCANCODE_C:
            case SDL_SCANCODE_U:
                if (down) g_pad1_state |= (1 << 1); else g_pad1_state &= ~(1 << 1); break; /* Y (Dash / Long pass) */
            case SDL_SCANCODE_RSHIFT:
            case SDL_SCANCODE_SPACE:
                if (down) g_pad1_state |= (1 << 2); else g_pad1_state &= ~(1 << 2); break; /* Select */
            case SDL_SCANCODE_RETURN:
                if (down) g_pad1_state |= (1 << 3); else g_pad1_state &= ~(1 << 3); break; /* Start (Pause) */
            case SDL_SCANCODE_UP:
            case SDL_SCANCODE_W:
                if (down) g_pad1_state |= (1 << 4); else g_pad1_state &= ~(1 << 4); break; /* Up */
            case SDL_SCANCODE_DOWN:
            case SDL_SCANCODE_S:
                if (down) g_pad1_state |= (1 << 5); else g_pad1_state &= ~(1 << 5); break; /* Down */
            case SDL_SCANCODE_LEFT:
            case SDL_SCANCODE_A:
                if (down) g_pad1_state |= (1 << 6); else g_pad1_state &= ~(1 << 6); break; /* Left */
            case SDL_SCANCODE_RIGHT:
            case SDL_SCANCODE_D:
                if (down) g_pad1_state |= (1 << 7); else g_pad1_state &= ~(1 << 7); break; /* Right */
            case SDL_SCANCODE_X:
            case SDL_SCANCODE_K:
                if (down) g_pad1_state |= (1 << 8); else g_pad1_state &= ~(1 << 8); break; /* A (Shoot / Confirm) */
            case SDL_SCANCODE_V:
            case SDL_SCANCODE_I:
                if (down) g_pad1_state |= (1 << 9); else g_pad1_state &= ~(1 << 9); break; /* X (Through ball / Lob) */
            case SDL_SCANCODE_Q:
                if (down) g_pad1_state |= (1 << 10); else g_pad1_state &= ~(1 << 10); break; /* L (Camera / Tactics) */
            case SDL_SCANCODE_E:
                if (down) g_pad1_state |= (1 << 11); else g_pad1_state &= ~(1 << 11); break; /* R (Strategy) */
            default:
                break;
        }
    }
}


#define MAX_INTERNAL_SCALE 8
#define MAX_INTERNAL_WIDTH  (MAX_WS_WIDTH * MAX_INTERNAL_SCALE)   /* 446 * 8 = 3568 */
#define MAX_INTERNAL_HEIGHT (SNES_HEIGHT * MAX_INTERNAL_SCALE)    /* 224 * 8 = 1792 */

static uint32_t g_hi_pixel_buffer[MAX_INTERNAL_WIDTH * MAX_INTERNAL_HEIGHT];

static void GetInternalResolutionDimensions(IssdInternalResolution res, int base_w, int base_h, int *out_w, int *out_h) {
    int scale = 1;
    switch (res) {
        case ISSD_RES_1X: scale = 1; break;
        case ISSD_RES_2X: scale = 2; break;
        case ISSD_RES_3X: scale = 3; break;
        case ISSD_RES_4X: scale = 4; break;
        case ISSD_RES_6X: scale = 6; break;
        case ISSD_RES_8X_4K: scale = 8; break;
        default: scale = 1; break;
    }
    if (out_w) *out_w = base_w * scale;
    if (out_h) *out_h = base_h * scale;
}

static void UpscaleFrameBuffer(uint32_t *dst, int dst_w, int dst_h, const uint32_t *src, int src_w, int src_h, IssdScalingFilter filter) {
    if (dst_w == src_w && dst_h == src_h) {
        memcpy(dst, src, (size_t)src_w * src_h * sizeof(uint32_t));
        return;
    }

    int scale_x = dst_w / src_w;
    int scale_y = dst_h / src_h;

    if (scale_x * src_w == dst_w && scale_y * src_h == dst_h) {
        /* Exact integer scaling */
        for (int y = 0; y < src_h; y++) {
            const uint32_t *src_row = &src[y * src_w];
            for (int sy = 0; sy < scale_y; sy++) {
                uint32_t *dst_row = &dst[(y * scale_y + sy) * dst_w];
                for (int x = 0; x < src_w; x++) {
                    uint32_t color = src_row[x];
                    if (filter == ISSD_FILTER_CRT && (sy % 2 != 0)) {
                        /* CRT scanline darkening */
                        uint32_t r = ((color >> 16) & 0xFF) * 3 / 4;
                        uint32_t g = ((color >> 8) & 0xFF) * 3 / 4;
                        uint32_t b = (color & 0xFF) * 3 / 4;
                        color = 0xFF000000 | (r << 16) | (g << 8) | b;
                    }
                    for (int sx = 0; sx < scale_x; sx++) {
                        dst_row[x * scale_x + sx] = color;
                    }
                }
            }
        }
    } else {
        /* Fallback nearest-neighbor for non-integer */
        for (int y = 0; y < dst_h; y++) {
            int src_y = (y * src_h) / dst_h;
            const uint32_t *src_row = &src[src_y * src_w];
            uint32_t *dst_row = &dst[y * dst_w];
            for (int x = 0; x < dst_w; x++) {
                int src_x = (x * src_w) / dst_w;
                dst_row[x] = src_row[src_x];
            }
        }
    }
}

static bool SaveFrame(const char *path, const uint32_t *native, int w, int h) {
    if (!issd_hd_active()) return SaveBmp(path, native, w, h);
    int scale = g_capture_scale;
    while (scale > 1 && (w * scale > MAX_INTERNAL_WIDTH || h * scale > MAX_INTERNAL_HEIGHT))
        scale--;
    UpscaleFrameBuffer(g_hi_pixel_buffer, w * scale, h * scale, native, w, h,
                       ISSD_FILTER_NEAREST);
    issd_hd_composite(g_snes ? g_snes->ppu : NULL, native, w, h,
                      g_hi_pixel_buffer, scale,
                      g_ws_active ? g_ws_extra : 0);
    return SaveBmp(path, g_hi_pixel_buffer, w * scale, h * scale);
}

/* Per-side widescreen margin in pixels. One definition: this used to be an
 * if-chain copied at three call sites, which is how they drift apart. */
static int IssdWsExtraForAspect(void) {
    if (!g_issd_config.true_widescreen) return 0;
    switch (g_issd_config.aspect_ratio) {
        case ISSD_ASPECT_AUTHENTIC: return 32; /* 320x224, no frozen edge players */
        case ISSD_ASPECT_16_10:     return 51; /* 358x224 */
        case ISSD_ASPECT_16_9:      return 71; /* 398x224 */
        case ISSD_ASPECT_21_9:      return 95; /* 446x224 */
        default:                    return 0;
    }
}

static void CalculateViewport(int win_w, int win_h, IssdAspectRatio aspect, int render_w, int render_h, SDL_Rect *out_rect) {
    if (!out_rect) return;
    if (win_w <= 0 || win_h <= 0) {
        out_rect->x = 0; out_rect->y = 0;
        out_rect->w = win_w; out_rect->h = win_h;
        return;
    }

    if (aspect == ISSD_ASPECT_INTEGER) {
        int scale_x = win_w / render_w;
        int scale_y = win_h / render_h;
        int scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (scale < 1) scale = 1;
        out_rect->w = render_w * scale;
        out_rect->h = render_h * scale;
        out_rect->x = (win_w - out_rect->w) / 2;
        out_rect->y = (win_h - out_rect->h) / 2;
        return;
    }

    float target_aspect;
    switch (aspect) {
        case ISSD_ASPECT_8_7:
            target_aspect = 8.0f / 7.0f; /* 1.142857: 1:1 pixel aspect */
            break;
        case ISSD_ASPECT_16_9:
            target_aspect = 16.0f / 9.0f;
            break;
        case ISSD_ASPECT_16_10:
            target_aspect = 16.0f / 10.0f;
            break;
        case ISSD_ASPECT_21_9:
            target_aspect = 21.0f / 9.0f;
            break;
        case ISSD_ASPECT_4_3:
        default:
            target_aspect = 4.0f / 3.0f; /* 1.333333 */
            break;
    }

    /* When True Widescreen is active and buffer is expanded, adhere to render buffer aspect */
    if (g_issd_config.true_widescreen && render_w > SNES_WIDTH) {
        target_aspect = (float)render_w / (float)render_h;
    }

    float win_aspect = (float)win_w / (float)win_h;
    if (win_aspect > target_aspect) {
        /* Window is wider than target aspect ratio (Pillarbox on left/right) */
        out_rect->h = win_h;
        out_rect->w = (int)(win_h * target_aspect + 0.5f);
        out_rect->x = (win_w - out_rect->w) / 2;
        out_rect->y = 0;
    } else {
        /* Window is taller than target aspect ratio (Letterbox on top/bottom) */
        out_rect->w = win_w;
        out_rect->h = (int)(win_w / target_aspect + 0.5f);
        out_rect->x = 0;
        out_rect->y = (win_h - out_rect->h) / 2;
    }
}

#if defined(__ANDROID__) || defined(ISSD_ANDROID)
DECLSPEC int SDL_main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
    g_argc = argc; g_argv = argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#ifdef ISSD_ANDROID
    /* A handheld may have both a touchscreen and physical controls; the
     * overlay is additive, so leaving it on costs nothing but screen. */
    issd_touch_set_enabled(true);
#else
    issd_touch_set_enabled(false);
#endif
#ifdef _WIN32
    SetUnhandledExceptionFilter(CrashFilter);
#else
    signal(SIGSEGV, CrashSignalHandler);
    signal(SIGBUS,  CrashSignalHandler);
    signal(SIGFPE,  CrashSignalHandler);
    signal(SIGILL,  CrashSignalHandler);
#endif
    const char *cli_config_path = NULL;
    const char *cli_rom_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            g_headless = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                g_target_frames = atoi(argv[++i]);
            } else {
                g_target_frames = 120;
            }
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            g_target_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            g_screenshot_path = argv[++i];
        } else if (strcmp(argv[i], "--dump-state") == 0 && i + 1 < argc) {
            g_dump_state_path = argv[++i];
        } else if (strcmp(argv[i], "--auto-start") == 0 && i + 1 < argc) {
            g_auto_start_frame = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--save-state") == 0 && i + 1 < argc) {
            g_save_state_frame = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--load-state") == 0 && i + 1 < argc) {
            g_load_state_frame = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            g_script_path = argv[++i];
        } else if (strcmp(argv[i], "--hd-pack") == 0 && i + 1 < argc) {
            g_hd_pack_dir = argv[++i];
        } else if (strcmp(argv[i], "--dump-tiles") == 0 && i + 1 < argc) {
            g_hd_dump_dir = argv[++i];
        } else if (strcmp(argv[i], "--dump-tiles-from") == 0 && i + 1 < argc) {
            g_hd_dump_from = (unsigned)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--capture-scale") == 0 && i + 1 < argc) {
            g_capture_scale = atoi(argv[++i]);
            if (g_capture_scale < 1) g_capture_scale = 1;
        } else if (strcmp(argv[i], "--dump-frames") == 0 && i + 1 < argc) {
            sscanf(argv[++i], "%d:%d", &g_dump_first, &g_dump_last);
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            cli_config_path = argv[++i];
        } else if (argv[i][0] != '-') {
            cli_rom_path = argv[i];
        }
    }

    ApplyDefaultAotBootDenySet(argc > 0 ? argv[0] : NULL);

    char config_path[1024];
    ResolveConfigPath(config_path, sizeof(config_path), cli_config_path);
    issd_config_set_default_path(config_path);
    issd_config_load(&g_issd_config, NULL);

    issd_save_init();
    issd_save_set_snapshot_backends(RtlSaveSnapshot, RtlLoadSnapshot);
    issd_mod_init();
    const char *mods_dir = "mods";
#ifdef ISSD_ANDROID
    mods_dir = issd_android_mods_dir();
#endif
    issd_mod_scan_and_load(mods_dir);
    /* Replacement background tiles. The command line wins over the saved
     * setting so a pack can be tried without committing to it, and the
     * dump directory is set before the first frame so the very first
     * screen's tiles are captured too. */
    {
        issd_hd_scan_packs(mods_dir);
        if (g_hd_pack_dir) {
            /* A directory named on the command line is used on its own,
             * so a pack can be tried without touching the saved stack. */
            issd_hd_load_pack(g_hd_pack_dir);
        } else {
            issd_hd_enable_from_list(g_issd_config.hd_texture_packs);
            issd_hd_apply(mods_dir);
        }
        issd_mod_result_note_tiles(issd_hd_active() ? issd_hd_texture_count() : 0);
        if (g_hd_dump_dir) {
            issd_hd_set_dump_dir(g_hd_dump_dir);
            issd_hd_set_dump_start(g_hd_dump_from);
            printf("[HD] Dumping background tiles to '%s'.\n", g_hd_dump_dir);
        }
    }
    issd_menu_init();

    char rom_path_buffer[ISSD_CONFIG_ROM_PATH_MAX];
    rom_path_buffer[0] = '\0';
    bool picked_rom = false;
    if (cli_rom_path && cli_rom_path[0]) {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", cli_rom_path);
#ifdef ISSD_ANDROID
    } else if (!g_headless && PromptForRomFile(rom_path_buffer, sizeof(rom_path_buffer))) {
        picked_rom = true;
    } else if (g_issd_config.rom_path[0] && FileExists(g_issd_config.rom_path)) {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", g_issd_config.rom_path);
    } else if (FileExists(DEFAULT_ROM_PATH)) {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", DEFAULT_ROM_PATH);
#else
    } else if (g_issd_config.rom_path[0] && FileExists(g_issd_config.rom_path)) {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", g_issd_config.rom_path);
    } else if (FileExists(DEFAULT_ROM_PATH)) {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", DEFAULT_ROM_PATH);
    } else if (!g_headless && PromptForRomFile(rom_path_buffer, sizeof(rom_path_buffer))) {
        picked_rom = true;
#endif
    } else if (g_issd_config.rom_path[0]) {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", g_issd_config.rom_path);
    } else {
        snprintf(rom_path_buffer, sizeof(rom_path_buffer), "%s", DEFAULT_ROM_PATH);
    }
    if (rom_path_buffer[0]) {
        snprintf(g_issd_config.rom_path, sizeof(g_issd_config.rom_path), "%s", rom_path_buffer);
        if (picked_rom) issd_config_save(&g_issd_config, NULL);
    }
    const char *rom_path = rom_path_buffer;

    printf("====================================================\n");
    printf("  ISSD Native — International Superstar Soccer Deluxe\n");
    printf("  Target: Windows x86-64 | Static Recompilation\n");
    printf("  Engine Mode: %s\n", (g_issd_config.engine_mode == ISSD_MODE_ENHANCED) ? "ENHANCED (60 Hz Smooth)" : "CLASSIC");
    printf("====================================================\n");
    printf("[Init] Loading ROM: %s\n", rom_path);

    size_t rom_size = 0;
    uint8_t *rom_data = LoadRomFile(rom_path, &rom_size);
    if (!rom_data) {
        fprintf(stderr, "[ERROR] Failed to read ROM file '%s'\n", rom_path);
#ifdef _WIN32
        if (!g_headless) {
            MessageBoxA(NULL,
                        "Failed to read the selected SNES ROM. Please choose a valid ISS Deluxe cartridge dump on the next run.",
                        "ISSD Native", MB_ICONERROR | MB_OK);
        }
#endif
        return 1;
    }
    printf("[Init] ROM read successfully (%zu bytes)\n", rom_size);
    g_rom_data = rom_data;
    g_rom_size = rom_size;

    /* Rosters are read from the cartridge image at runtime, so mods are
     * applied to it here: after the ROM is in memory, before the engine
     * boots and reads any of it. The pristine copy lets the pack be changed
     * later from the menu without stacking one patch on top of another. */
    issd_mod_rom_set_image(rom_data, rom_size);
    issd_mod_enable_from_list(g_issd_config.active_mod_packs);
    issd_mod_reapply();

    /* Mods take effect during a restart, which is exactly when nobody is
     * watching a console. Report the outcome on screen - and do it after
     * everything has actually been applied, or the counts are all zero. */
    {
        char summary[96];
        /* Re-applying the roster stack resets the counters, and the tile
         * stack was loaded before that, so its total is restated here. */
        issd_mod_result_note_tiles(issd_hd_texture_count());
        issd_mod_result_summary(summary, sizeof summary);
        printf("[ModLoader] %s\n", summary);
        issd_menu_notify(summary, 300);
    }

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    g_apu_mutex = SDL_CreateMutex();
    if (!g_apu_mutex) {
        fprintf(stderr, "[ERROR] SDL_CreateMutex failed: %s\n", SDL_GetError());
        return 1;
    }

    RtlRegisterGame(&kIssdGameInfo);
    Snes *snes = SnesInit(rom_data, (int)rom_size);
    if (!snes) {
        fprintf(stderr, "[ERROR] SnesInit failed!\n");
        free(rom_data);
        return 1;
    }
    printf("[Init] SnesInit succeeded! Recompiled CPU and SNES hardware runtime online.\n");

    /* Execute the SNES boot sequence from Reset vector ($80:8000) */
    IssdBootReset();

    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Texture *source_texture = NULL;
    int source_width = 0, source_height = 0;
    int cur_tex_w = 0, cur_tex_h = 0;
    IssdScalingFilter cur_filter = g_issd_config.scaling_filter;
    SDL_AudioDeviceID audio_dev = 0;

    if (!g_headless) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "[ERROR] SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError());
            return 1;
        }

        Uint32 win_flags = SDL_WINDOW_RESIZABLE;
        if (g_issd_config.fullscreen) {
            win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }

        int win_w = (g_issd_config.window_width > 0) ? g_issd_config.window_width : DEFAULT_WINDOW_WIDTH;
        int win_h = (g_issd_config.window_height > 0) ? g_issd_config.window_height : DEFAULT_WINDOW_HEIGHT;

        g_window = SDL_CreateWindow(
            "ISSD Native — International Superstar Soccer Deluxe",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            win_w, win_h,
            win_flags
        );
        if (!g_window) {
            fprintf(stderr, "[ERROR] SDL_CreateWindow failed: %s\n", SDL_GetError());
            return 1;
        }

        Uint32 ren_flags = SDL_RENDERER_ACCELERATED;
        if (g_issd_config.vsync) {
            ren_flags |= SDL_RENDERER_PRESENTVSYNC;
        }

        renderer = SDL_CreateRenderer(g_window, -1, ren_flags);
        if (!renderer) {
            renderer = SDL_CreateRenderer(g_window, -1, 0);
        }

        int cur_ws_extra = IssdWsExtraForAspect();
        g_ws_extra = cur_ws_extra;
        g_ws_active = (cur_ws_extra > 0);
        int cur_render_w = SNES_WIDTH + 2 * cur_ws_extra;
        int cur_render_h = SNES_HEIGHT;

        if (g_issd_config.scaling_filter == ISSD_FILTER_CRT) {
            GetInternalResolutionDimensions(g_issd_config.internal_res, cur_render_w, cur_render_h, &cur_tex_w, &cur_tex_h);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING,
                cur_tex_w, cur_tex_h
            );
        } else {
            cur_tex_w = cur_render_w;
            cur_tex_h = cur_render_h;
            const char *filter_hint = (g_issd_config.scaling_filter == ISSD_FILTER_LINEAR) ? "1" : "0";
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter_hint);
        }

        int req_freq = (g_issd_config.audio_freq >= 8000 && g_issd_config.audio_freq <= 192000)
                       ? g_issd_config.audio_freq : AUDIO_FREQ;

        SDL_AudioSpec wanted_spec = {
            .freq = req_freq,
            .format = AUDIO_S16SYS,
            .channels = AUDIO_CHANNELS,
            .samples = AUDIO_SAMPLES,
            .callback = SdlAudioCallback,
            .userdata = NULL,
        };
        SDL_AudioSpec obtained_spec;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &obtained_spec, 0);
        if (audio_dev) {
            RtlSetAudioOutputRate(obtained_spec.freq);
            g_audio_frames_per_block = (534 * obtained_spec.freq + 32040 / 2) / 32040;
            s_audio_block_avail = 0;
            s_audio_block_pos = 0;
            SDL_PauseAudioDevice(audio_dev, 0);
            printf("[Audio] SDL Audio Device opened (%d Hz, %d channels, %d samples, %d per block)\n",
                   obtained_spec.freq, obtained_spec.channels, obtained_spec.samples, g_audio_frames_per_block);
        } else {
            printf("[Audio] Warning: Could not open audio device: %s\n", SDL_GetError());
        }
    } else {
        printf("[Init] Running in HEADLESS mode for %d frames.\n", g_target_frames);
    }

    uint32_t frame_count = 0;
    IssdMatchState match_state;
    memset(&match_state, 0, sizeof(match_state));

    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    uint64_t sim_interval = perf_freq / 60;
    uint64_t next_sim_time = SDL_GetPerformanceCounter();
    uint64_t next_render_time = next_sim_time;
    uint64_t last_present_time = next_sim_time;
    bool audio_paused = false;

    printf("[Running] Starting main execution loop...\n");

    if (g_script_path) issd_script_load(g_script_path);

    while (g_running) {
        if (!g_headless) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) {
                    g_running = false;
                } else {
                    ProcessInputEvent(&ev);
                }
            }
            PumpTouchState();
        }

        uint64_t now = SDL_GetPerformanceCounter();
        if (audio_dev && audio_paused != issd_menu_is_open()) {
            audio_paused = issd_menu_is_open();
            SDL_PauseAudioDevice(audio_dev, audio_paused);
        }

        int cur_ws_extra = IssdWsExtraForAspect();
        g_ws_extra = cur_ws_extra;
        g_ws_active = (cur_ws_extra > 0);
        int cur_render_w = SNES_WIDTH + 2 * cur_ws_extra;
        int cur_render_h = SNES_HEIGHT;

        /* Simulation Tick: Paced deterministically at 60 Hz */
        bool frame_simulated = false;
        if (g_restart_requested) {
            g_restart_requested = false;
            printf("[ISSD Native] Performing in-process soft reset...\n");
            issd_mod_enable_from_list(g_issd_config.active_mod_packs);
            issd_mod_reapply();
            char summary[96];
            issd_mod_result_note_tiles(issd_hd_texture_count());
            issd_mod_result_summary(summary, sizeof(summary));
            issd_menu_notify(summary, 300);
            IssdBootReset();
            frame_count = 0;
            continue;
        }

        if (g_headless || now >= next_sim_time) {
            if (issd_menu_is_open()) {
                /* Paused: Render In-Game Menu Overlay */
                issd_menu_render(g_pixel_buffer, cur_render_w, cur_render_h);
                frame_simulated = true;
            } else {
                /* Clear frame buffer & set PPU draw buffer */
                memset(g_pixel_buffer, 0, (size_t)cur_render_w * cur_render_h * sizeof(uint32_t));
                PpuBeginDrawing(g_snes->ppu, (uint8_t *)g_pixel_buffer, (size_t)cur_render_w * sizeof(uint32_t),
                    kPpuRenderFlags_NewRenderer | (g_ws_active ? kPpuRenderFlags_NoSpriteLimits : 0));

                if (issd_script_active()) {
                    g_pad1_state = issd_script_mask(frame_count);
                } else if (g_auto_start_frame > 0) {
                    g_pad1_state = 0;
                    if (frame_count >= (uint32_t)g_auto_start_frame) {
                        uint32_t phase = frame_count % 60;
                        if (g_ram[0x70] == 0x08) {
                            /* Once the match is live, never pulse Start: that
                             * pauses the simulation and makes a frozen-frame
                             * soak test meaningless. Keep players moving and
                             * exercising passes/shots instead. */
                            if (phase < 10) {
                                g_pad1_state |= (1 << 8); /* A: Shoot */
                            } else if (phase >= 20 && phase < 45) {
                                g_pad1_state |= (1 << 0) | (1 << 7); /* B + Right */
                            }
                        } else if (phase < 15) {
                            g_pad1_state |= (1 << 8); /* A: Confirm */
                        } else if (phase < 25) {
                            g_pad1_state |= (1 << 3); /* Start: advance menus */
                        } else if (phase < 45) {
                            g_pad1_state |= (1 << 0) | (1 << 7); /* B + Right */
                        }
                    }
                }

                /* Install a banked state before the frame that consumes it, so
                 * the restored WRAM and video memory drive this frame whole. */
                if (g_load_state_frame >= 0 &&
                    frame_count == (uint32_t)g_load_state_frame) {
                    if (!issd_load_quick()) Die("--load-state: no quicksave to restore");
                }

                /* Run 1 SNES frame */
                /* Touch is additive: a physical pad and the on-screen pad
                 * both drive player 1, which is what a handheld with both
                 * needs. */
                uint32_t touch_bits = issd_touch_pad_mask();
                uint32_t inputs = ((g_pad1_state | touch_bits) & 0xFFF)
                                | ((g_pad2_state & 0xFFF) << 12);
                uint64_t _rf_t0 = SDL_GetPerformanceCounter();
                RtlRunFrame(inputs);
                uint64_t _rf_t1 = SDL_GetPerformanceCounter();
                double _rf_sec = (double)(_rf_t1 - _rf_t0) / perf_freq;
                if (_rf_sec > 0.05) {
                    fprintf(stderr, "[SlowFrame %u] took %.3fs\n", frame_count, _rf_sec);
                }

                if (g_save_state_frame >= 0 &&
                    frame_count == (uint32_t)g_save_state_frame) {
                    if (!issd_save_quick()) Die("--save-state: could not write quicksave");
                }
                if (g_dump_state_path && (g_cpu.S != 0x1af || cpu_read16(&g_cpu, 0, 0x3c))) {
                    static unsigned reports;
                    if (reports++ < 12)
                        fprintf(stderr, "[FrameCPU %u] S=%04X PB=%02X busy=%04X\n",
                            frame_count, g_cpu.S, g_cpu.PB, cpu_read16(&g_cpu, 0, 0x3c));
                }
                if (!audio_dev) {
                    static int16_t dummy_audio[534 * 2];
                    RtlRenderAudio(dummy_audio, 534, AUDIO_CHANNELS);
                }

                /* Render SNES PPU scanlines */
                IssdDrawPpuFrame();
                /* Over the game, not inside the menu: the one thing the
                 * player has to see after a restart is whether the mods
                 * they restarted for actually applied. */
                issd_menu_render_notification(g_pixel_buffer, cur_render_w,
                                              cur_render_h);
                issd_menu_render_stadium_plate(g_pixel_buffer, cur_render_w,
                                              cur_render_h,
                                              g_ws_active ? g_ws_extra : 0);
                issd_menu_render_team_photo(g_pixel_buffer, cur_render_w,
                                            cur_render_h,
                                            g_ws_active ? g_ws_extra : 0);
                issd_menu_render_team_grid(g_pixel_buffer, cur_render_w,
                                           cur_render_h,
                                           g_ws_active ? g_ws_extra : 0);
                issd_menu_render_team_plate(g_pixel_buffer, cur_render_w,
                                            cur_render_h,
                                            g_ws_active ? g_ws_extra : 0);
                if (g_dump_first >= 0 && (int)frame_count >= g_dump_first && (int)frame_count <= g_dump_last) {
                    char nm[64];
                    snprintf(nm, sizeof(nm), "f_%05u.bmp", frame_count);
                    SaveFrame(nm, g_pixel_buffer, cur_render_w, cur_render_h);
                }


                /* Scanline Filter Effect (Native buffer mode) */
                if (g_issd_config.scanlines && g_issd_config.internal_res == ISSD_RES_1X) {
                    for (int y = 1; y < cur_render_h; y += 2) {
                        for (int x = 0; x < cur_render_w; x++) {
                            uint32_t p = g_pixel_buffer[y * cur_render_w + x];
                            uint32_t r = ((p >> 16) & 0xFF) * 3 / 4;
                            uint32_t g = ((p >> 8) & 0xFF) * 3 / 4;
                            uint32_t b = (p & 0xFF) * 3 / 4;
                            g_pixel_buffer[y * cur_render_w + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
                        }
                    }
                }

                frame_count++;
                frame_simulated = true;

                /* Periodically save debug screenshots in headless mode */
                if (g_headless && (frame_count % 120 == 0)) {
                    char scr_name[64];
                    snprintf(scr_name, sizeof(scr_name), "test_step_%04u.bmp", frame_count);
                    SaveFrame(scr_name, g_pixel_buffer, cur_render_w, cur_render_h);
                }

                /* Check match state every 60 frames */
                if (frame_count % 60 == 0) {
                    issd_bridge_update_state(&match_state);
                    printf("[Frame %u] ", frame_count);
                    issd_bridge_log_state(&match_state);
                    if (match_state.game_mode1 >= 0x01) {
                        issd_mod_apply_match_overrides(match_state.p1_team, match_state.p2_team);
                    }
                    fflush(stdout);
                }
            }

            next_sim_time += sim_interval;
            if (now > next_sim_time && (now - next_sim_time) > sim_interval * 4) {
                next_sim_time = now;
            }
        }

        /* Catch up simulation before spending another tick presenting. A slow
         * upload or VSync must not permanently reduce the audio producer rate. */
        now = SDL_GetPerformanceCounter();
        /* Presentation Tick: Render frame with aspect ratio, internal resolution, and target FPS */
        if (!g_headless) {
            uint64_t render_interval = (g_issd_config.target_fps > 0) ? (perf_freq / g_issd_config.target_fps) : 0;
            if (frame_simulated || issd_presentation_due(now, next_sim_time, last_present_time,
                                      next_render_time, render_interval,
                                      sim_interval * 4)) {
                /* Check if internal resolution or filter hint changed. Nearest
                 * and linear present the logical SNES/widescreen buffer
                 * directly; the selected filter is applied by SDL while copying
                 * to the window. CRT keeps a scaled texture because scanline
                 * darkening is generated into that buffer. */
                int target_tex_w = cur_render_w, target_tex_h = cur_render_h;
                if (g_issd_config.scaling_filter == ISSD_FILTER_CRT || issd_hd_active()) {
                    GetInternalResolutionDimensions(g_issd_config.internal_res, cur_render_w, cur_render_h, &target_tex_w, &target_tex_h);
                }

                if (target_tex_w != cur_tex_w || target_tex_h != cur_tex_h || cur_filter != g_issd_config.scaling_filter) {
                    if (texture) { SDL_DestroyTexture(texture); texture = NULL; }
                    cur_tex_w = target_tex_w;
                    cur_tex_h = target_tex_h;
                    cur_filter = g_issd_config.scaling_filter;

                    const char *filter_hint = (cur_filter == ISSD_FILTER_LINEAR) ? "1" : "0";
                    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter_hint);

                    if (cur_filter == ISSD_FILTER_CRT) {
                        texture = SDL_CreateTexture(
                            renderer,
                            SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING,
                            cur_tex_w, cur_tex_h
                        );
                    }
                }

                /* Replacement tiles are written into the scaled buffer, so
                 * a pack forces the scaled-texture path even when the
                 * filter would otherwise hand SDL the native frame. */
                SDL_Texture *present_texture = NULL;
                if (cur_filter == ISSD_FILTER_CRT || issd_hd_active()) {
                    if (!texture) {
                        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, cur_tex_w, cur_tex_h);
                    }
                    UpscaleFrameBuffer(g_hi_pixel_buffer, cur_tex_w, cur_tex_h,
                                   g_pixel_buffer, cur_render_w, cur_render_h,
                                   g_issd_config.scaling_filter);
                    if (issd_hd_active() && cur_tex_w % cur_render_w == 0)
                        issd_hd_composite(g_snes->ppu, g_pixel_buffer,
                                          cur_render_w, cur_render_h,
                                          g_hi_pixel_buffer,
                                          cur_tex_w / cur_render_w,
                                          g_ws_active ? g_ws_extra : 0);
                    SDL_UpdateTexture(texture, NULL, g_hi_pixel_buffer, cur_tex_w * sizeof(uint32_t));
                    present_texture = texture;
                } else {
                    if (!source_texture || source_width != cur_render_w || source_height != cur_render_h) {
                        if (source_texture) SDL_DestroyTexture(source_texture);
                        source_width = cur_render_w; source_height = cur_render_h;
                        source_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, source_width, source_height);
                    }
                    if (!source_texture) Die(SDL_GetError());
                    SDL_UpdateTexture(source_texture, NULL, g_pixel_buffer, cur_render_w * sizeof(uint32_t));
                    present_texture = source_texture;
                }

                /* Calculate non-stretched viewport according to window size and chosen aspect ratio */
                int win_w = 0, win_h = 0;
                SDL_GetWindowSize(g_window, &win_w, &win_h);
                SDL_Rect dst_rect;
                CalculateViewport(win_w, win_h, g_issd_config.aspect_ratio, cur_render_w, cur_render_h, &dst_rect);

                /* Clear borders to solid black (pillarbox / letterbox) */
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, present_texture, NULL, &dst_rect);
                RenderTouchOverlay(renderer);
                SDL_RenderPresent(renderer);
                last_present_time = SDL_GetPerformanceCounter();

                if (render_interval > 0) {
                    next_render_time += render_interval;
                    if (now > next_render_time && (now - next_render_time) > render_interval * 4) {
                        next_render_time = now;
                    }
                }
            }

            /* Low-overhead sleep when ahead of next simulation or presentation tick without VSync */
            if (!g_issd_config.vsync) {
                now = SDL_GetPerformanceCounter();
                uint64_t earliest = next_sim_time;
                if (render_interval > 0 && next_render_time < earliest) {
                    earliest = next_render_time;
                }
                if (earliest > now) {
                    uint32_t delay_ms = (uint32_t)((earliest - now) * 1000 / perf_freq);
                    if (delay_ms > 0) SDL_Delay(delay_ms);
                }
            }
        }

        if (g_target_frames > 0 && (int)frame_count >= g_target_frames) {
            printf("[Done] Target frame count reached: %u frames\n", frame_count);
            break;
        }
    }

    if (g_screenshot_path) {
        int cur_ws_extra = IssdWsExtraForAspect();
        int cur_render_w = SNES_WIDTH + 2 * cur_ws_extra;
        if (SaveFrame(g_screenshot_path, g_pixel_buffer, cur_render_w, SNES_HEIGHT)) {
            printf("[Screenshot] Saved frame buffer to: %s (%dx%d)\n", g_screenshot_path, cur_render_w, SNES_HEIGHT);
        } else {
            fprintf(stderr, "[Screenshot] Failed to save screenshot to: %s\n", g_screenshot_path);
        }
    }

    if (g_dump_state_path) {
        char path[1024];
        snprintf(path, sizeof(path), "%s.wram", g_dump_state_path);
        FILE *dump = fopen(path, "wb");
        if (dump) { fwrite(g_ram, 1, 0x20000, dump); fclose(dump); }
        snprintf(path, sizeof(path), "%s.ppu", g_dump_state_path);
        dump = fopen(path, "wb");
        if (dump) { fwrite(g_snes->ppu, 1, sizeof(*g_snes->ppu), dump); fclose(dump); }
        printf("[CPU] S=%04X PB=%02X NMI-busy=%04X\n", g_cpu.S, g_cpu.PB,
               cpu_read16(&g_cpu, 0, 0x3c));
        RecompStackBalDumpStderr(20);
    }
    printf("[Shutdown] Executed %u frames total. Shutting down...\n", frame_count);
    issd_config_save(&g_issd_config, NULL);

    if (!g_headless) {
        if (audio_dev) SDL_CloseAudioDevice(audio_dev);
        if (texture) SDL_DestroyTexture(texture);
        if (source_texture) SDL_DestroyTexture(source_texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (g_window) SDL_DestroyWindow(g_window);
        SDL_Quit();
    }

    if (g_apu_mutex) SDL_DestroyMutex(g_apu_mutex);
    free(rom_data);
    printf("[Shutdown] Clean exit complete.\n");
    return 0;
}
