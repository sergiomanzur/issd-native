from pathlib import Path
import subprocess
import pytest
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_intro_bypass_and_snapshot_reattachment_preserve_guest_memory(tmp_path):
    root = Path(__file__).resolve().parents[1]
    native = root/'ISSDNative'
    runtime = root/'deps/snesrecomp/runner/src'
    rom = root/'International Superstar Soccer Deluxe (USA).sfc'
    if not rom.exists():
        pytest.skip('Requires private cartridge')
    assert 'issd_stadium_scene_restore' in (native/'issd_stadium_scene.h').read_text(), 'Snapshot scene reattachment missing'
    mapper = (runtime/'snes/cart.c').read_text()
    harness = '''
#include "issd_stadium_scene.h"
#include "issd_stadium_assets.h"
#include "snes/cart.h"
#include "snes/ppu.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static IssdStadiumProfile profile = {.version=1,.base_layout=0,.length_units=1728,.width_units=576};
static IssdStadiumAssets art;
static bool artwork_enabled;
const IssdStadiumProfile *issd_stadium_profile(unsigned id){return id==8 ? &profile : NULL;}
const IssdStadiumAssets *issd_stadium_assets(unsigned id){return artwork_enabled && id==8 ? &art : NULL;}
unsigned issd_stadium_generation(void){return 1;}
bool issd_stadium_trace_enabled(void){return false;}
bool issd_stadium_has_profiles(void){return true;}
uint32_t sdd1_mmc_offset(Sdd1 *chip,uint32_t address){(void)chip;(void)address;return UINT32_MAX;}
uint32_t sdd1_lorom_window_offset(Sdd1 *chip,uint8_t bank,uint16_t address){(void)chip;(void)bank;(void)address;return UINT32_MAX;}
uint8_t *sa1_cpu_memory_ptr(Sa1 *chip,uint8_t bank,uint16_t address){(void)chip;(void)bank;(void)address;return NULL;}
'''
    for declaration in ('bool cart_setRomView(', 'void cart_clearRomView(', 'uint8_t *cart_getRomPtr('):
        harness += function(mapper,declaration)
    harness += '''
int main(int argc,char **argv){
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 uint8_t *rom=malloc(0x200000),*ram=calloc(1,0x20000),*saved=malloc(0x20000);assert(rom&&ram&&saved);
 assert(fread(rom,1,0x200000,f)==0x200000);fclose(f);
 Cart cart={0};cart.type=CART_LOROM;cart.rom=rom;cart.romSize=0x200000;
 ram[0x1fa2]=8;ram[0x70]=12;
 assert(issd_stadium_scene_opcode(&cart,ram,rom,0x200000,0x85a50a));assert(!cart.romView);
 ram[0x70]=15;assert(issd_stadium_scene_opcode(&cart,ram,rom,0x200000,0x83b165));assert(!cart.romView);
 ram[0x70]=4;ram[0x86]=0;assert(issd_stadium_scene_opcode(&cart,ram,rom,0x200000,0x8b8000));
 assert(cart.romView); /* view must precede original camera/table initialization */
 uint8_t *camera=cart_getRomPtr(&cart,0x81,0xef21);assert(camera[0]==0xa0&&camera[1]==4);
 assert(issd_stadium_scene_opcode(&cart,ram,rom,0x200000,0x8b85e3));
 uint8_t *ptr=cart_getRomPtr(&cart,0x81,0xec47);assert(ptr[0]==0xc0&&ptr[1]==6);
 ram[0x70]=8;ram[0x18000]=0x5a;memcpy(saved,ram,0x20000);cart_clearRomView(&cart);
 assert(issd_stadium_scene_restore(&cart,ram,rom,0x200000));
 ptr=cart_getRomPtr(&cart,0x81,0xec47);assert(ptr[0]==0xc0&&ptr[1]==6);
 assert(memcmp(ram,saved,0x20000)==0);issd_stadium_scene_reset(&cart);assert(!cart.romView);
 Ppu *ppu=calloc(1,sizeof *ppu);assert(ppu);memset(ppu->vram,0xa5,sizeof ppu->vram);
 uint8_t pixels[32];memset(pixels,0x66,sizeof pixels);
 IssdStadiumTileWrite write={.word_destination=0x4000,.size=32,.data=pixels};
 art.writes=&write;art.write_count=1;art.palette_size=64;memset(art.palette,0x22,64);artwork_enabled=true;
 ram[0x100]=2;issd_stadium_scene_transfer(ppu,ram,0x80b909);
 assert(((uint8_t*)ppu->vram)[0x8000]==0xa5);
 ram[0x100]=0;ram[0x130]=5;issd_stadium_scene_transfer(ppu,ram,0x80b909);
 assert(((uint8_t*)ppu->vram)[0x8000]==0xa5);
 ram[0x130]=0;issd_stadium_scene_transfer(ppu,ram,0x80b909);
 assert(((uint8_t*)ppu->vram)[0x8000]==0xa5); /* outside measured gameplay allocation */
 ppu->bgmode=1;ppu->bgTileAdr=0x22;
 issd_stadium_scene_transfer(ppu,ram,0x80b909);
 assert(memcmp((uint8_t*)ppu->vram+0x8000,pixels,32)==0);
 assert(((uint8_t*)ppu->vram)[0x7fff]==0xa5&&((uint8_t*)ppu->vram)[0x8020]==0xa5);
 memset(ram+0x2c00,0x33,1024);issd_stadium_scene_transfer(ppu,ram,0x8b8dc0);
 assert(memcmp(ram+0x2c40,art.palette,64)==0&&ram[0x2c3f]==0x33&&ram[0x2c80]==0x33&&ram[0x2e40]==0x33);
 memset(ppu->vram,0xa5,sizeof ppu->vram);ram[0x86]=8;
 issd_stadium_scene_transfer(ppu,ram,0x80b909);assert(((uint8_t*)ppu->vram)[0x8000]==0xa5);
 /* A valid authored map does not need the default transform to fit. Layout1
  * at1568 exhausts the default transform's metatile budget. */
 issd_stadium_scene_reset(&cart);profile.base_layout=1;profile.length_units=1568;profile.width_units=640;
 memset(&art.geometry,0,sizeof art.geometry);art.geometry.base_layout=1;art.geometry.length=1568;
 art.geometry.width=640;art.geometry.stride=704;ram[0x86]=1;ram[0x70]=4;
 assert(issd_stadium_scene_opcode(&cart,ram,rom,0x200000,0x8b8000));assert(cart.romView);
 memcpy(saved,ram,0x20000);cart_clearRomView(&cart);
 assert(issd_stadium_scene_restore(&cart,ram,rom,0x200000));assert(!memcmp(saved,ram,0x20000));
 uint8_t identity[16];issd_stadium_scene_art_identity(ram,identity);
 issd_stadium_scene_saved_art(identity,true);
 issd_stadium_scene_refresh_art(ppu,ram);assert(!memcmp(saved,ram,0x20000));
 art.palette[0]^=0x1f;
 issd_stadium_scene_saved_art(identity,true);
 ram[0x11a0]=2;memcpy(saved,ram,0x20000);
 issd_stadium_scene_refresh_art(ppu,ram);assert(!memcmp(saved,ram,0x20000));
 ram[0x11a0]=0;ram[0x100]=1;memcpy(saved,ram,0x20000);
 issd_stadium_scene_refresh_art(ppu,ram);assert(!memcmp(saved,ram,0x20000));
 ram[0x100]=0;ppu->bgmode=1;ppu->bgTileAdr=0x22;
 issd_stadium_scene_refresh_art(ppu,ram);
 assert(!memcmp(ram+0x2c40,art.palette,64));
 assert(!memcmp(ram+0x2e40,art.palette,64));assert(ram[0x41]&0x80);
 assert(!memcmp(ram+0x18000,art.geometry.metatiles[0],8192));
 memcpy(saved,ram,0x20000);issd_stadium_scene_refresh_art(ppu,ram);
 assert(!memcmp(saved,ram,0x20000)); /* refresh once; original animation resumes */
 profile.base_layout=0;profile.length_units=1728;profile.width_units=576;
 issd_stadium_scene_art_identity(ram,identity);artwork_enabled=false;
 issd_stadium_scene_saved_art(identity,true);
 assert(issd_stadium_scene_restore(&cart,ram,rom,0x200000));
 memset((uint8_t*)ppu->vram+0x8000,0xdd,32);
 issd_stadium_scene_refresh_art(ppu,ram);
 assert(((uint8_t*)ppu->vram)[0x8000]!=0xdd);
 assert(memcmp(ram+0x2c40,art.palette,64));
 issd_stadium_scene_reset(&cart);
 free(ppu);free(rom);free(ram);free(saved);return 0;
}
'''
    test = tmp_path/'scene.c'
    test.write_text(harness)
    decoder = tmp_path/'decoder.c'
    decoder.write_text('#include "issd_decompress.h"\n#include <string.h>\n#define WINDOW_SIZE 0x400\n#define WINDOW_MASK 0x3ff\n'+
        function((native/'issd_decompress.c').read_text(),'ISSD_Decompress('))
    executable = tmp_path/'scene.exe'
    compile_c(executable,root,[test,decoder,native/'issd_stadium_scene.c',native/'issd_stadium_rom.c',
        native/'issd_stadium_geometry.c',native/'issd_stadium_template.c'],[native,runtime])
    result = subprocess.run([str(executable),str(rom)],capture_output=True,text=True)
    assert result.returncode == 0,result.stdout+result.stderr
