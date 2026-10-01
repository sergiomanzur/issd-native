#include <assert.h>
#include "issd_script.h"
int main(int argc, char **argv) {
    assert(argc == 2);
    assert(issd_script_load(argv[1]) == 5);
    assert(issd_script_players() == 15);
    assert(issd_script_mask(10) == 1);
    assert(issd_script_mask_player(10, 1) == 256);
    assert(issd_script_mask_player(10, 2) == 512);
    assert(issd_script_mask_player(10, 3) == 128);
    assert(issd_script_mask_player(20, 1) == 0);
    assert(issd_script_mask_player(20, 2) == 512);
    assert(issd_script_mask_player(20, 4) == 0);
    return 0;
}
