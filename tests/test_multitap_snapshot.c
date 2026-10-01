/* Only unrelated component serializers are omitted. The SNES serializer below
 * is compiled directly from production source by test_local_multiplayer.py. */
void cpu_saveload(Cpu *p, SaveLoadInfo *s) { (void)p; (void)s; }
void apu_saveload(Apu *p, SaveLoadInfo *s) { (void)p; (void)s; }
void dma_saveload(Dma *p, SaveLoadInfo *s) { (void)p; (void)s; }
void ppu_saveload(Ppu *p, SaveLoadInfo *s) { (void)p; (void)s; }
void cart_saveload(Cart *p, SaveLoadInfo *s) { (void)p; (void)s; }

typedef struct {
    SaveLoadInfo base;
    unsigned char bytes[0x21000];
    size_t position;
    bool loading;
} Buffer;

static void transfer(SaveLoadInfo *info, void *data, size_t size) {
    Buffer *buffer = (Buffer *)info;
    assert(buffer->position + size <= sizeof(buffer->bytes));
    if (buffer->loading) memcpy(data, buffer->bytes + buffer->position, size);
    else memcpy(buffer->bytes + buffer->position, data, size);
    buffer->position += size;
}

int main(void) {
    Snes snes = {0};
    Cpu cpu = {0};
    unsigned char ram[0x20000] = {0};
    Buffer current = {{transfer}, {0}, 0, false};
    Buffer legacy = {{transfer}, {0}, 0, false};
    snes.cpu = &cpu;
    snes.ram = ram;
    snes.joypadMultitap = true;
    snes.joypadConnected = 15;
    snes.joypadIo = 127;
    snes.joypadPair2Index = 7;
    snes.joypad3Latched = 512;
    snes.joypad4Latched = 128;
    snes.joypadAutoWords[3] = 64;
    snes_saveload_set_version(8);
    snes_saveload(&snes, &current.base);
    snes_saveload_set_version(7);
    snes_saveload(&snes, &legacy.base);
    assert(current.position == legacy.position + 16);
    assert(memcmp(current.bytes, legacy.bytes, legacy.position) == 0);
    snes.joypadMultitap = false;
    snes.joypadConnected = 0;
    snes.joypadIo = 255;
    snes.joypadPair2Index = 16;
    snes.joypad3Latched = snes.joypad4Latched = 0;
    memset(snes.joypadAutoWords, 0, sizeof(snes.joypadAutoWords));
    current.loading = true;
    current.position = 0;
    snes_saveload_set_version(8);
    snes_saveload(&snes, &current.base);
    assert(snes.joypadMultitap && snes.joypadConnected == 15);
    assert(snes.joypadIo == 127 && snes.joypadPair2Index == 7);
    assert(snes.joypad3Latched == 512 && snes.joypad4Latched == 128);
    assert(snes.joypadAutoWords[3] == 64);
    legacy.loading = true;
    legacy.position = 0;
    snes_saveload_set_version(7);
    snes_saveload(&snes, &legacy.base);
    assert(snes.joypadMultitap && snes.joypadConnected == 15);
    assert(snes.joypadIo == 255 && snes.joypadPair2Index == 0);
    assert(snes.joypad3Latched == 0 && snes.joypad4Latched == 0);
    assert(snes.joypadAutoWords[3] == 0);
    return 0;
}
