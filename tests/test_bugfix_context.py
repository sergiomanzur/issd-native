"""The master bug-fix setting changes snapshot compatibility and cache eligibility."""
from pathlib import Path
import subprocess
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_bugfix_setting_refreshes_save_context(tmp_path):
    root = Path(__file__).resolve().parents[1]
    main = (root / "ISSDNative/main.c").read_text()
    source = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include "issd_config.h"
IssdConfig g_issd_config;
static uint8_t rom[1];
static uint8_t *g_base_rom_data = rom, *g_rom_data = rom;
static size_t g_rom_size = 1;
static uint32_t g_save_gameplay_flags = UINT32_MAX;
static uint32_t save_flags, password_flags;
static unsigned configure_count, cache_resets, campaign_resets, continue_refreshes;
static int native_enabled;
static void IssdGameplayNativeBlock(void) {}
static void cpu_set_native_block_hook(void (*hook)(void)) { native_enabled = hook != NULL; }
static void IssdConfigureGameplayHooks(void) { configure_count++; }
static void issd_bugfix_keeper_begin_loop(void) {}
static void issd_save_set_context(const void *a, size_t b, const void *c, size_t d, uint32_t flags) {
    (void)a;(void)b;(void)c;(void)d;save_flags = flags;
}
static void issd_password_set_context(const void *a, size_t b, const void *c, size_t d, uint32_t flags) {
    (void)a;(void)b;(void)c;(void)d;password_flags = flags;
}
static void issd_match_reset(void) { cache_resets++; }
static void issd_campaign_reset(void) { campaign_resets++; }
static void issd_menu_refresh_continue(void) { continue_refreshes++; }
''' + function(main, "static void IssdRefreshSaveContext(") + r'''
int main(void) {
    IssdRefreshSaveContext(); assert(save_flags == 0 && !native_enabled);
    g_issd_config.gameplay_bug_fixes = true;
    IssdRefreshSaveContext(); assert(save_flags == 8 && password_flags == 8 && native_enabled);
    assert(configure_count == 2 && cache_resets == 2 && campaign_resets == 2 && continue_refreshes == 2);
    IssdRefreshSaveContext(); assert(configure_count == 2 && cache_resets == 2);
    g_issd_config.gameplay_goalkeeper_ai = g_issd_config.gameplay_player_ai = true;
    IssdRefreshSaveContext(); assert(save_flags == 14);
    g_issd_config.gameplay_bug_fixes = false;
    IssdRefreshSaveContext(); assert(save_flags == 6 && native_enabled);
    g_issd_config.gameplay_goalkeeper_ai = g_issd_config.gameplay_player_ai = false;
    IssdRefreshSaveContext(); assert(save_flags == 0 && !native_enabled);
    return 0;
}
'''
    path = tmp_path / "context.c"
    path.write_text(source)
    exe = tmp_path / "context.exe"
    compile_c(exe, root, [path], [root / "ISSDNative"])
    result = subprocess.run([str(exe)], cwd=tmp_path, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
