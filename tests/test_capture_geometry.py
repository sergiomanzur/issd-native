"""Paused captures retain the captured game's stride, even after an aspect edit."""
from pathlib import Path
import subprocess

from test_config_persistence import compile_c
from test_renderer_reset import braced_block

ROOT = Path(__file__).resolve().parents[1]


def test_hd_capture_uses_captured_margin(tmp_path):
    source = (ROOT / "ISSDNative/main.c").read_text()
    function = braced_block(source, source.index("static bool SaveFrame(const char *path, const uint32_t *native, int w, int h) {"))
    harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define MAX_INTERNAL_WIDTH 4080
#define MAX_INTERNAL_HEIGHT 1792
#define SNES_WIDTH 256
#define ISSD_FILTER_NEAREST 0
static int g_capture_scale=8;
bool g_ws_active=false;
int g_ws_extra=0;
static uint32_t g_hi_pixel_buffer[1];
static bool hd_active;
static int captured_w,captured_h,composite_margin,composite_scale;
typedef struct { void *ppu; } Snes;
static Snes *g_snes;
static bool issd_hd_active(void) { return hd_active; }
static bool SaveBmp(const char *p,const uint32_t *pixels,int w,int h) {
    (void)p;(void)pixels;captured_w=w;captured_h=h;return true;
}
static void UpscaleFrameBuffer(uint32_t *dest,int dw,int dh,const uint32_t *src,int sw,int sh,int f) {
    (void)dest;(void)src;(void)f;assert(dw%sw==0 && dh%sh==0);
}
static void issd_hd_composite(void *p,const uint32_t *src,int w,int h,uint32_t *dst,int scale,int margin) {
    (void)p;(void)src;(void)w;(void)h;(void)dst;composite_margin=margin;composite_scale=scale;
}
'''
    checks = r'''
int main(void) {
    uint32_t frame[1]={0};
    hd_active=false; assert(SaveFrame("test",frame,398,224));
    assert(captured_w==398 && captured_h==224);
    hd_active=true; assert(SaveFrame("test",frame,398,224));
    assert(composite_margin==71 && composite_scale==8 && captured_w==3184);
    assert(SaveFrame("test",frame,256,224)); assert(composite_margin==0);
    assert(SaveFrame("test",frame,504,224)); assert(composite_margin==124 && captured_w==4032);
    g_capture_scale=16;
    assert(SaveFrame("test",frame,504,224)); assert(composite_scale==8);
    return 0;
}
'''
    c = tmp_path / "capture.c"
    c.write_text(harness + function + checks)
    exe = tmp_path / "capture.exe"
    compile_c(exe, ROOT, [c], [ROOT / "ISSDNative"])
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    assert run.returncode == 0, run.stdout + run.stderr
