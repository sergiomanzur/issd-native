#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SDL_MAIN_HANDLED
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
#include "issd_frame_pacing.h"

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
static uint32_t g_pixel_buffer[MAX_WS_WIDTH * SNES_HEIGHT];
static uint32_t g_pad1_state = 0;
static uint32_t g_pad2_state = 0;
static bool g_running = true;
static bool g_headless = false;
static int g_target_frames = -1;
static const char *g_screenshot_path = NULL;
static const char *g_dump_state_path = NULL;
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

/* Audio parameters */
#define AUDIO_FREQ 44100
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES 1024

static void SDLCALL SdlAudioCallback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    int samples = len / (sizeof(int16_t) * AUDIO_CHANNELS);
    int16_t *buf = (int16_t *)stream;
    RtlRenderAudio(buf, samples, AUDIO_CHANNELS);
    int vol = g_issd_config.master_volume;
    if (vol < 100 && vol >= 0) {
        for (int i = 0; i < samples * AUDIO_CHANNELS; i++) {
            buf[i] = (int16_t)(((int32_t)buf[i] * vol) / 100);
        }
    }
}

static void IssdDrawPpuFrame(void) {
    if (!g_snes || !g_snes->ppu) return;

    issd_widescreen_begin(g_snes->ppu, g_ram, g_rom_data, g_rom_size,
                         g_ws_active ? g_ws_extra : 0);

    for (int line = 0; line < SNES_HEIGHT; line++) {
        ppu_runLine(g_snes->ppu, line);
    }
    ppu_handleVblank(g_snes->ppu);
    issd_widescreen_end(g_snes->ppu);
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
    printf("[ISSD Native] Reset sequence completed (result: %d, PB: $%02X, S: $%04X).\n",
           ok, (unsigned)g_cpu.PB, (unsigned)g_cpu.S);
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

    /* Execute 1 frame via NMI interrupt handler at $80:80E0 */
    interp_tier_dispatch_interrupt(&g_cpu, 0x8080E0);
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

static SDL_GameController *g_controller = NULL;
static SDL_Window *g_window = NULL;

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
        if (down && (btn == SDL_CONTROLLER_BUTTON_GUIDE)) {
            issd_menu_toggle();
            return;
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

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#ifdef _WIN32
    SetUnhandledExceptionFilter(CrashFilter);
#endif
    /* Initialize subsystems */
    issd_config_load(&g_issd_config, "issd_config.json");
    issd_save_init();
    issd_mod_init();
    issd_mod_scan_and_load("mods");
    issd_menu_init();

    const char *rom_path = DEFAULT_ROM_PATH;
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
        } else if (argv[i][0] != '-') {
            rom_path = argv[i];
        }
    }

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
        return 1;
    }
    printf("[Init] ROM read successfully (%zu bytes)\n", rom_size);
    g_rom_data = rom_data;
    g_rom_size = rom_size;

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

        int cur_ws_extra = 0;
        if (g_issd_config.true_widescreen) {
            if (g_issd_config.aspect_ratio == ISSD_ASPECT_16_9) {
                cur_ws_extra = 71;
            } else if (g_issd_config.aspect_ratio == ISSD_ASPECT_16_10) {
                cur_ws_extra = 51;
            } else if (g_issd_config.aspect_ratio == ISSD_ASPECT_21_9) {
                cur_ws_extra = 95;
            }
        }
        g_ws_extra = cur_ws_extra;
        g_ws_active = (cur_ws_extra > 0);
        int cur_render_w = SNES_WIDTH + 2 * cur_ws_extra;
        int cur_render_h = SNES_HEIGHT;

        GetInternalResolutionDimensions(g_issd_config.internal_res, cur_render_w, cur_render_h, &cur_tex_w, &cur_tex_h);
        const char *filter_hint = (g_issd_config.scaling_filter == ISSD_FILTER_NEAREST) ? "0" :
                                  (g_issd_config.scaling_filter == ISSD_FILTER_LINEAR) ? "1" : "0";
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter_hint);

        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            g_issd_config.scaling_filter == ISSD_FILTER_CRT ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_TARGET,
            cur_tex_w, cur_tex_h
        );

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
            SDL_PauseAudioDevice(audio_dev, 0);
            printf("[Audio] SDL Audio Device opened (%d Hz, %d channels, %d samples)\n",
                   obtained_spec.freq, obtained_spec.channels, obtained_spec.samples);
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
        }

        uint64_t now = SDL_GetPerformanceCounter();
        if (audio_dev && audio_paused != issd_menu_is_open()) {
            audio_paused = issd_menu_is_open();
            SDL_PauseAudioDevice(audio_dev, audio_paused);
        }

        int cur_ws_extra = 0;
        if (g_issd_config.true_widescreen) {
            if (g_issd_config.aspect_ratio == ISSD_ASPECT_16_9) {
                cur_ws_extra = 71;
            } else if (g_issd_config.aspect_ratio == ISSD_ASPECT_16_10) {
                cur_ws_extra = 51;
            } else if (g_issd_config.aspect_ratio == ISSD_ASPECT_21_9) {
                cur_ws_extra = 95;
            }
        }
        g_ws_extra = cur_ws_extra;
        g_ws_active = (cur_ws_extra > 0);
        int cur_render_w = SNES_WIDTH + 2 * cur_ws_extra;
        int cur_render_h = SNES_HEIGHT;

        /* Simulation Tick: Paced deterministically at 60 Hz */
        if (g_headless || now >= next_sim_time) {
            if (issd_menu_is_open()) {
                /* Paused: Render In-Game Menu Overlay */
                issd_menu_render(g_pixel_buffer, cur_render_w, cur_render_h);
            } else {
                /* Clear frame buffer & set PPU draw buffer */
                memset(g_pixel_buffer, 0, (size_t)cur_render_w * cur_render_h * sizeof(uint32_t));
                PpuBeginDrawing(g_snes->ppu, (uint8_t *)g_pixel_buffer, (size_t)cur_render_w * sizeof(uint32_t),
                    g_ws_active ? kPpuRenderFlags_NewRenderer | kPpuRenderFlags_NoSpriteLimits : 0);

                if (g_auto_start_frame > 0) {
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

                /* Run 1 SNES frame */
                uint32_t inputs = (g_pad1_state & 0xFFF) | ((g_pad2_state & 0xFFF) << 12);
                RtlRunFrame(inputs);
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

                /* Periodically save debug screenshots in headless mode */
                if (g_headless && (frame_count % 120 == 0)) {
                    char scr_name[64];
                    snprintf(scr_name, sizeof(scr_name), "test_step_%04u.bmp", frame_count);
                    SaveBmp(scr_name, g_pixel_buffer, cur_render_w, cur_render_h);
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
            if (issd_presentation_due(now, next_sim_time, last_present_time,
                                      next_render_time, render_interval,
                                      sim_interval * 4)) {
                /* Check if internal resolution or filter hint changed */
                int target_tex_w = 0, target_tex_h = 0;
                GetInternalResolutionDimensions(g_issd_config.internal_res, cur_render_w, cur_render_h, &target_tex_w, &target_tex_h);

                if (target_tex_w != cur_tex_w || target_tex_h != cur_tex_h || cur_filter != g_issd_config.scaling_filter) {
                    if (texture) SDL_DestroyTexture(texture);
                    cur_tex_w = target_tex_w;
                    cur_tex_h = target_tex_h;
                    cur_filter = g_issd_config.scaling_filter;

                    const char *filter_hint = (cur_filter == ISSD_FILTER_NEAREST) ? "0" :
                                              (cur_filter == ISSD_FILTER_LINEAR) ? "1" : "0";
                    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, filter_hint);

                    texture = SDL_CreateTexture(
                        renderer,
                        SDL_PIXELFORMAT_ARGB8888,
                        cur_filter == ISSD_FILTER_CRT ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_TARGET,
                        cur_tex_w, cur_tex_h
                    );
                }

                if (cur_filter == ISSD_FILTER_CRT) {
                    UpscaleFrameBuffer(g_hi_pixel_buffer, cur_tex_w, cur_tex_h,
                                   g_pixel_buffer, cur_render_w, cur_render_h,
                                   g_issd_config.scaling_filter);
                    SDL_UpdateTexture(texture, NULL, g_hi_pixel_buffer, cur_tex_w * sizeof(uint32_t));
                } else {
                    /* Upload original pixels once; the renderer performs the
                     * integer expansion. CPU expansion/upload of a 4K buffer
                     * starved simulation and caused audible PCM underruns. */
                    if (!source_texture || source_width != cur_render_w || source_height != cur_render_h) {
                        if (source_texture) SDL_DestroyTexture(source_texture);
                        source_width = cur_render_w; source_height = cur_render_h;
                        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
                        source_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, source_width, source_height);
                    }
                    if (!texture || !source_texture || SDL_SetRenderTarget(renderer, texture) != 0)
                        Die(SDL_GetError());
                    SDL_UpdateTexture(source_texture, NULL, g_pixel_buffer, cur_render_w * sizeof(uint32_t));
                    SDL_RenderCopy(renderer, source_texture, NULL, NULL);
                    SDL_SetRenderTarget(renderer, NULL);
                }

                /* Calculate non-stretched viewport according to window size and chosen aspect ratio */
                int win_w = 0, win_h = 0;
                SDL_GetWindowSize(g_window, &win_w, &win_h);
                SDL_Rect dst_rect;
                CalculateViewport(win_w, win_h, g_issd_config.aspect_ratio, cur_render_w, cur_render_h, &dst_rect);

                /* Clear borders to solid black (pillarbox / letterbox) */
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &dst_rect);
                SDL_RenderPresent(renderer);
                last_present_time = SDL_GetPerformanceCounter();

                if (render_interval > 0) {
                    next_render_time += render_interval;
                    if (now > next_render_time && (now - next_render_time) > render_interval * 4) {
                        next_render_time = now;
                    }
                }
            }

            /* Low-overhead sleep when ahead of next simulation or presentation tick */
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

        if (g_target_frames > 0 && (int)frame_count >= g_target_frames) {
            printf("[Done] Target frame count reached: %u frames\n", frame_count);
            break;
        }
    }

    if (g_screenshot_path) {
        int cur_ws_extra = 0;
        if (g_issd_config.true_widescreen) {
            if (g_issd_config.aspect_ratio == ISSD_ASPECT_16_9) {
                cur_ws_extra = 71;
            } else if (g_issd_config.aspect_ratio == ISSD_ASPECT_16_10) {
                cur_ws_extra = 51;
            } else if (g_issd_config.aspect_ratio == ISSD_ASPECT_21_9) {
                cur_ws_extra = 95;
            }
        }
        int cur_render_w = SNES_WIDTH + 2 * cur_ws_extra;
        if (SaveBmp(g_screenshot_path, g_pixel_buffer, cur_render_w, SNES_HEIGHT)) {
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
