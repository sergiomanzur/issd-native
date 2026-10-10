#include "issd_stadium_assets.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    IssdStadiumAssets *output = NULL;
    bool valid = atoi(argv[2]) != 0;
    assert(issd_stadium_assets_load(argv[1], 0, &output) == valid);
    if (!valid) { assert(!output); return 0; }
    assert(output && output->write_count == 1 && output->palette_size == 96);
    assert(output->writes[0].word_destination == 0x4000);
    assert(output->writes[0].size == 32);
    assert(!memcmp(output->writes[0].data, (uint8_t[32]){0}, 32));
    IssdStadiumAssets *previous = output;
    assert(!issd_stadium_assets_load(argv[1], 1, &output));
    assert(output == previous);
    issd_stadium_assets_free(output);
    return 0;
}
