"""Compile and exercise the host's actual reset event and resource cleanup."""
from pathlib import Path
import subprocess

from test_config_persistence import compile_c


ROOT = Path(__file__).resolve().parents[1]


def braced_block(source, start):
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def test_renderer_reset_events_invalidate_every_cached_texture(tmp_path):
    source = (ROOT / "ISSDNative/main.c").read_text(encoding="utf-8")
    event_start = source.index("if (ev->type == SDL_RENDER_DEVICE_RESET")
    event_guard = braced_block(source, event_start)
    cleanup_start = source.index("if (g_renderer_reset_pending && renderer)")
    cleanup = braced_block(source, cleanup_start)
    harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
enum { SDL_RENDER_TARGETS_RESET=0x2000, SDL_RENDER_DEVICE_RESET=0x2001 };
typedef struct { uint32_t type; } SDL_Event;
typedef struct { unsigned id; } SDL_Texture;
static bool g_renderer_reset_pending;
static int *renderer;
static SDL_Texture *texture, *source_texture, *menu_texture;
static int cur_tex_w,cur_tex_h,source_width,source_height,menu_width,menu_height;
static unsigned renderer_resets, destroyed;
static void SDL_DestroyTexture(SDL_Texture *value) {
    assert(value && value->id < 3);
    assert(!(destroyed & (1u << value->id)));
    destroyed |= 1u << value->id;
    free(value);
}
static SDL_Texture *make_texture(unsigned id) {
    SDL_Texture *value=malloc(sizeof *value); assert(value);
    value->id=id; return value;
}
'''
    checks = r'''
int main(void) {
    int real_renderer=1;
    renderer=&real_renderer;
    texture=make_texture(0); source_texture=make_texture(1); menu_texture=make_texture(2);
    cur_tex_w=768;cur_tex_h=672;source_width=256;source_height=224;
    menu_width=1920;menu_height=1080;
    SDL_Event event={SDL_RENDER_DEVICE_RESET};
    ProcessInputEvent(&event);
    assert(g_renderer_reset_pending && destroyed==0);
    event.type=SDL_RENDER_TARGETS_RESET;
    ProcessInputEvent(&event); /* duplicate events coalesce before next present */
    assert(g_renderer_reset_pending && renderer_resets==0);
    CleanupRendererReset();
    assert(!g_renderer_reset_pending && renderer_resets==1 && destroyed==7);
    assert(!texture && !source_texture && !menu_texture);
    assert(!cur_tex_w && !cur_tex_h && !source_width && !source_height);
    assert(!menu_width && !menu_height);
    CleanupRendererReset(); assert(renderer_resets==1); /* no double destroy */
    destroyed=0;
    source_texture=make_texture(1);source_width=256;source_height=224;
    ProcessInputEvent(&event);
    renderer=NULL;
    CleanupRendererReset();
    assert(g_renderer_reset_pending && source_texture && destroyed==0);
    renderer=&real_renderer;
    CleanupRendererReset();
    assert(!g_renderer_reset_pending && !source_texture && destroyed==2 && renderer_resets==2);
    assert(!source_width && !source_height);
    return 0;
}
'''
    path = tmp_path / "renderer_reset.c"
    path.write_text(
        harness
        + "\nstatic void ProcessInputEvent(const SDL_Event *ev) {\n"
        + event_guard
        + '\nassert(!"Reset events must return before input or simulation callbacks");\n}\n'
        + "static void CleanupRendererReset(void) {\n"
        + cleanup
        + "\n}\n"
        + checks,
        encoding="utf-8",
    )
    exe = tmp_path / "renderer_reset.exe"
    compile_c(exe, ROOT, [path], [])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
