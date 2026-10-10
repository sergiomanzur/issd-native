#include "issd_asset_path.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    char path[4096], output[4096];
    snprintf(path, sizeof path, "%s/asset.bin", argv[1]);
    FILE *file = fopen(path, "wb"); assert(file);
    fputc(1, file); fclose(file);
    assert(issd_asset_resolve(argv[1], "asset.bin", output, sizeof output));
    file = fopen(output, "rb"); assert(file); assert(fgetc(file) == 1); fclose(file);
    const char *bad[] = {"../asset.bin", "/asset.bin", "D:/asset.bin",
        "asset\\bin", "missing.bin", "", "./", "asset.bin/"};
    for (unsigned i = 0; i < sizeof bad/sizeof bad[0]; ++i)
        assert(!issd_asset_resolve(argv[1], bad[i], output, sizeof output));
    assert(!issd_asset_resolve(argv[1], "asset.bin", output, 2));
    return 0;
}
