"""Exercise the host's actual keyboard and lifecycle glue with controlled input."""
from pathlib import Path
import subprocess
from test_config_persistence import compile_c

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('static ', source.index(name) - 40)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def test_keyboard_and_background_pause(tmp_path):
    source = (ROOT / 'ISSDNative/main.c').read_text()
    bodies = '\n'.join(function(source, name) for name in
                       ('IssdBlockKeyboard(', 'IssdKeyboardRead(', 'IssdHandleLifecycleEvent('))
    harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "issd_config.h"
typedef unsigned int Uint32;
typedef unsigned char Uint8;
enum { SDL_NUM_SCANCODES=512, SDL_APP_WILLENTERBACKGROUND=1,
       SDL_APP_DIDENTERBACKGROUND=2, SDL_APP_WILLENTERFOREGROUND=3, SDL_APP_DIDENTERFOREGROUND=4 };
static bool g_app_background, g_input_focused=true;
static bool g_touch_release_guard;
static uint32_t g_pad1_state;
static bool s_keyboard_blocked[512];
static unsigned char key_state[512];
static int pauses, clears, saves;
static bool focused=true;
const unsigned char *SDL_GetKeyboardState(int *count) { if(count)*count=512; return key_state; }
void issd_input_set_focus(bool v) { focused=v; }
void issd_touch_reset_points(void) { clears++; }
void issd_menu_open(void) { pauses++; }
bool issd_config_save(const IssdConfig *cfg,const char *path) { (void)cfg;(void)path;saves++;return true; }
'''
    checks = r'''
int main(void) {
    memset(&g_issd_config,0,sizeof g_issd_config);
    g_issd_config.key_p1_b=29; g_issd_config.key_p1_a=27;
    key_state[29]=key_state[13]=1; /* Z and legacy J aliases */
    assert(IssdKeyboardRead()==1);
    key_state[29]=0; assert(IssdKeyboardRead()==1); /* releasing an alias cannot cancel the other */
    g_issd_config.key_p1_b=5; /* remapped B */
    assert(IssdKeyboardRead()==0);
    key_state[5]=1; assert(IssdKeyboardRead()==1);
    IssdBlockKeyboard(); assert(IssdKeyboardRead()==0);
    key_state[5]=0; IssdKeyboardRead(); key_state[5]=1; assert(IssdKeyboardRead()==1);
    g_issd_config.key_p1_b=-200; g_issd_config.key_p1_a=2147483647;
    assert(IssdKeyboardRead()==0); /* hostile config cannot index outside SDL key state */
    assert(!IssdHandleLifecycleEvent(99));
    assert(IssdHandleLifecycleEvent(SDL_APP_WILLENTERBACKGROUND));
    assert(g_app_background && !g_input_focused && !focused && pauses==1 && clears==1 && saves==1);
    assert(IssdHandleLifecycleEvent(SDL_APP_DIDENTERBACKGROUND));
    assert(pauses==1 && saves==1);
    assert(IssdHandleLifecycleEvent(SDL_APP_WILLENTERFOREGROUND));
    assert(g_app_background && !focused); /* do not resume before foreground is complete */
    assert(IssdHandleLifecycleEvent(SDL_APP_DIDENTERFOREGROUND));
    assert(!g_app_background && g_input_focused && focused && pauses==1 && clears==2);
    assert(g_pad1_state==0); /* resident match stays paused for explicit resume */
    return 0;
}
'''
    path = tmp_path / 'host.c'
    path.write_text(harness + '\nIssdConfig g_issd_config;\n' + bodies + checks)
    exe = tmp_path / 'host.exe'
    compile_c(exe, ROOT, [path], [ROOT / 'ISSDNative'])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
