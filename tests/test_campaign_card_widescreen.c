#define main original_fixture_main
#include "test_widescreen_native.c"
#undef main

int main(void) {
  const int extras[] = {51,71,124};
  for (unsigned i=0;i<3;i++) {
    fixture(); word(0x70,0x0c); word(0x1ffcc,0);
    ppu.bgmode=9; ppu.bgXsc[0]=1; ppu.bgXsc[1]=0x10;
    ppu.bgXsc[2]=9; ppu.bgTileAdr=0x4422;
    ppu.screenEnabled[0]=0x13; ppu.cgram[0]=0;
    assert(!issd_widescreen_menu_layout(&ppu,ram));
    issd_widescreen_begin(&ppu,ram,rom,sizeof rom,extras[i]);
    assert(ppu.wsLayerRepeat==0);
    issd_widescreen_end(&ppu);
    ppu.screenEnabled[0]=0x12; /* card with BG1 temporarily hidden */
    assert(!issd_widescreen_menu_layout(&ppu,ram));
    /* Wallpaper menus enable BG3; a fade to black must not change ownership. */
    ppu.screenEnabled[0]=0x17;
    assert(issd_widescreen_menu_layout(&ppu,ram));
  }
  puts("Campaign title/reflection layers stay centered; wallpaper remains wide");
}
