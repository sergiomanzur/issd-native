"""Exercise real mapper functions with a borrowed same-sized scene view."""
import subprocess
from pathlib import Path
from test_config_persistence import compile_c
from test_snapshot_transactional import function


def test_borrowed_lorom_view_preserves_owned_cartridge(tmp_path):
    root = Path(__file__).resolve().parents[1]
    runtime = root / 'deps/snesrecomp/runner/src'
    source = (runtime / 'snes/cart.c').read_text()
    assert 'bool cart_setRomView(' in source, 'Borrowed scene ROM view missing'
    harness = r'''
#include <assert.h>
#include "snes/cart.h"
uint32_t sdd1_mmc_offset(Sdd1 *chip, uint32_t address) {
    (void)chip; (void)address; return UINT32_MAX;
}
uint32_t sdd1_lorom_window_offset(Sdd1 *chip, uint8_t bank, uint16_t address) {
    (void)chip; (void)bank; (void)address; return UINT32_MAX;
}
uint8_t *sa1_cpu_memory_ptr(Sa1 *chip, uint8_t bank, uint16_t address) {
    (void)chip; (void)bank; (void)address; return NULL;
}
'''
    for declaration in ('bool cart_setRomView(', 'void cart_clearRomView(',
                        'uint8_t *cart_getRomPtr('):
        harness += function(source, declaration)
    harness += r'''
int main(void) {
    uint8_t canonical[0x10000] = {0}, view[0x10000] = {0};
    canonical[0x1234] = 0x11; canonical[0x1235] = 0x22;
    view[0x1234] = 0x33; view[0x1235] = 0x44;
    Cart cart = {0}; cart.type = CART_LOROM;
    cart.rom = canonical; cart.romSize = sizeof canonical;
    assert(cart_setRomView(&cart, view, sizeof view));
    uint8_t *ptr = cart_getRomPtr(&cart, 0x80, 0x9234);
    assert(ptr == view + 0x1234 && ptr[0] == 0x33 && ptr[1] == 0x44);
    assert(cart_getRomPtr(&cart, 0, 0x9234) == ptr);
    assert(cart.rom == canonical && canonical[0x1234] == 0x11);
    assert(!cart_setRomView(&cart, view, sizeof view - 1));
    assert(cart_getRomPtr(&cart, 0x80, 0x9234) == ptr);
    assert(!cart_setRomView(&cart, NULL, sizeof view));
    cart_clearRomView(&cart);
    assert(cart_getRomPtr(&cart, 0x80, 0x9234) == canonical + 0x1234);
    cart.type = CART_HIROM;
    assert(!cart_setRomView(&cart, view, sizeof view));
    assert(!cart_setRomView(NULL, view, sizeof view));
    return 0;
}
'''
    test = tmp_path / 'rom_view.c'
    test.write_text(harness)
    executable = tmp_path / 'rom_view.exe'
    compile_c(executable, root, [test], [runtime])
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
