#include "issd_password.h"
#include "sha256.h"
#include "snes/interp816.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Use the supported cartridge's actual encoder, decoder, checksum and field
 * tables on a private bus. The host never invents a second password schema.
 * ROM ownership remains with main; it must refresh this context after patches. */
static const uint8_t *s_rom;
static size_t s_rom_size;
static const char *s_error = "Password context unavailable";
static const char *s_context_error = "Password context unavailable";
static const uint8_t k_supported_sha256[32] = {
    0xd2,0xfe,0x66,0xc1,0xce,0x66,0xc6,0x5c,0xe1,0x4e,0x47,0x8c,0x94,0xbe,0x2e,0x61,
    0x6f,0x9e,0x2c,0xad,0x37,0x4b,0x57,0x83,0xa6,0xa6,0x4d,0x3c,0x1a,0x99,0xcf,0xa9
};
static const uint8_t k_glyphs[64] = {
    0x0c,0x0d,0x0e,0x10,0x11,0x12,0x14,0x15,0x16,0x17,0x18,0x1a,0x1b,0x1c,0x1d,0x1e,
    0x20,0x21,0x22,0x23,0x24,0x2b,0x2d,0x2f,0x30,0x31,0x33,0x37,0x3a,0x3b,0x3d,0x01,
    0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x26,0x47,0x59,0x5a,0x48,0x28,0x49,0x4a,
    0x4f,0x4b,0x25,0x54,0x4c,0x27,0x5b,0x5c,0x4d,0x4e,0x5d,0x5e,0x5f,0x60,0x61,0x62
};
static const char *const k_labels[64] = {
    "B","C","D","F","G","H","J","K","L","M","N","P","Q","R","S","T",
    "V","W","X","Y","Z","b","d","f","g","h","j","n","q","r","t","0",
    "1","2","3","4","5","6","7","8","-","+","DIV","PI","=","%","<",">",
    "~","$",":","\"","?","!","DOWN","UP","*","#","NOTE","STAR","SPADE","DIAMOND","CLUB","HEART"
};

typedef struct {
    uint8_t ram[ISSD_PASSWORD_RAM_SIZE];
    uint8_t io[32];
    bool fault;
} PasswordBus;

static bool Fail(const char *message) { s_error = message; return false; }
const char *issd_password_error(void) { return s_error; }
bool issd_password_available(void) { return s_rom != NULL; }
const char *issd_password_symbol_label(unsigned symbol) { return symbol < 64 ? k_labels[symbol] : "?"; }
uint8_t issd_password_glyph(unsigned symbol) { return symbol < 64 ? k_glyphs[symbol] : 0; }
int issd_password_symbol_from_char(int character) {
    if (character == '/') return 42; /* ASCII keyboard alias for division. */
    for (unsigned i = 0; i < 64; ++i)
        if (k_labels[i][0] == character && !k_labels[i][1]) return (int)i;
    return -1;
}

bool issd_password_set_context(const uint8_t *base, size_t base_size,
                              const uint8_t *effective, size_t effective_size,
                              uint32_t gameplay_flags) {
    s_rom = NULL; s_rom_size = 0;
    s_context_error = "Unsupported cartridge for retail passwords";
    if (!base || !effective || base_size != 0x200000 || effective_size != base_size)
        return Fail(s_context_error);
    uint8_t digest[32]; sha256_compute(base, base_size, digest);
    if (memcmp(digest,k_supported_sha256,sizeof(digest)))
        return Fail(s_context_error);
    if (gameplay_flags || memcmp(base,effective,base_size)) {
        s_context_error = "Retail passwords require unmodified gameplay data";
        return Fail(s_context_error);
    }
    s_rom = base; s_rom_size = base_size; s_error = ""; return true;
}

static unsigned Word(const uint8_t *ram, unsigned address) {
    return ram[address] | (unsigned)ram[address + 1] << 8;
}
static unsigned Pointer(const uint8_t *ram, unsigned address) {
    return Word(ram,address) | (unsigned)ram[address + 2] << 16;
}
static void PutWord(uint8_t *ram, unsigned address, unsigned value) {
    ram[address] = (uint8_t)value; ram[address + 1] = (uint8_t)(value >> 8);
}
static bool CampaignFlags(const uint8_t *ram) {
    unsigned flags = Word(ram,0x1648);
    /* Scenario ($1000), the $0400 mode and $0002 mode have different original
     * password tables/restore paths. Mixed mode bits are outside this bridge. */
    if (flags & 0x1402) return false;
    unsigned type = flags & 0x24;
    return type == 4 || type == 0x20;
}

static uint8_t ReadBus(void *opaque, uint32_t address) {
    PasswordBus *bus = opaque;
    unsigned bank = address >> 16, low = address & 0xffff;
    if (bank == 0x7e || bank == 0x7f) return bus->ram[((bank - 0x7e) << 16) | low];
    if (low < 0x2000 && (bank < 0x40 || (bank >= 0x80 && bank < 0xc0))) return bus->ram[low];
    if (low >= 0x4200 && low <= 0x421f && (bank < 0x40 || (bank >= 0x80 && bank < 0xc0)))
        return bus->io[low - 0x4200];
    size_t offset = ((size_t)(bank & 0x7f) << 15) | (low & 0x7fff);
    if (low < 0x8000 || offset >= s_rom_size) { bus->fault = true; return 0; }
    return s_rom[offset];
}
static void WriteBus(void *opaque, uint32_t address, uint8_t value) {
    PasswordBus *bus = opaque;
    unsigned bank = address >> 16, low = address & 0xffff;
    if (bank == 0x7e || bank == 0x7f) { bus->ram[((bank - 0x7e) << 16) | low] = value; return; }
    if (low < 0x2000 && (bank < 0x40 || (bank >= 0x80 && bank < 0xc0))) { bus->ram[low] = value; return; }
    if (low < 0x4200 || low > 0x421f || !(bank < 0x40 || (bank >= 0x80 && bank < 0xc0))) {
        bus->fault = true; return;
    }
    bus->io[low - 0x4200] = value;
    if (low == 0x4206) {
        unsigned dividend = bus->io[4] | (unsigned)bus->io[5] << 8;
        unsigned quotient = value ? dividend / value : 0xffff;
        unsigned remainder = value ? dividend % value : dividend;
        bus->io[0x14] = (uint8_t)quotient; bus->io[0x15] = (uint8_t)(quotient >> 8);
        bus->io[0x16] = (uint8_t)remainder; bus->io[0x17] = (uint8_t)(remainder >> 8);
    }
}

static bool Call(PasswordBus *bus, Interp816 *cpu, unsigned address) {
    cpu->k = (uint8_t)(address >> 16); cpu->pc = (uint16_t)address;
    cpu->sp = 0x1ffb; cpu->dp = 0; cpu->db = 0x81;
    bus->ram[0x1ffc] = 0xfe; bus->ram[0x1ffd] = 0xff; bus->ram[0x1ffe] = 0x7f;
    extern uint32_t g_interp816_cur_pc;
    uint32_t prior_pc = g_interp816_cur_pc;
    for (unsigned i = 0; i < 100000; ++i) {
        if (cpu->k == 0x7f && cpu->pc == 0xffff) {
            g_interp816_cur_pc = prior_pc;
            return !bus->fault || Fail("Password cartridge execution failed");
        }
        if (bus->fault || cpu->stopped || cpu->waiting) break;
        interp816_runOpcode(cpu);
    }
    g_interp816_cur_pc = prior_pc;
    return Fail("Password cartridge execution failed");
}

static PasswordBus *Stage(const uint8_t *ram, Interp816 *cpu) {
    PasswordBus *bus = calloc(1,sizeof(*bus));
    if (!bus) { Fail("Cannot allocate password staging state"); return NULL; }
    memcpy(bus->ram,ram,sizeof(bus->ram));
    memset(cpu,0,sizeof(*cpu)); cpu->mem = bus; cpu->read = ReadBus; cpu->write = WriteBus;
    return bus;
}

bool issd_password_encode(const uint8_t *ram, uint8_t *symbols, size_t capacity, size_t *count) {
    s_error = "";
    if (!s_rom) return Fail(s_context_error);
    if (!ram || !symbols || !count || !CampaignFlags(ram)) return Fail("No supported campaign to export");
    Interp816 cpu; PasswordBus *bus = Stage(ram,&cpu); if (!bus) return false;
    cpu.a = (uint16_t)Word(ram,0x1648);
    bool ok = Call(bus,&cpu,0xa4a9a3);
    size_t length = 0;
    if (ok && cpu.x < 24) length = ReadBus(bus,0x81e44d + cpu.x);
    if (!length || length > ISSD_PASSWORD_MAX_SYMBOLS || capacity < length) ok = Fail("Password output buffer too small or unsupported campaign");
    if (ok) ok = Call(bus,&cpu,0x83f46d);
    cpu.y = (uint16_t)length;
    if (ok) ok = Call(bus,&cpu,0x86cb64);
    if (ok) { memcpy(symbols,bus->ram + 0xe2d0,length); *count = length; }
    free(bus); return ok;
}

bool issd_password_decode(const uint8_t *symbols, size_t count, const uint8_t *ram, uint8_t *decoded_ram) {
    s_error = "";
    if (!s_rom) return Fail(s_context_error);
    if (!ram || !symbols || !count || count > ISSD_PASSWORD_MAX_SYMBOLS || decoded_ram == ram)
        return Fail("Invalid password input");
    for (size_t i = 0; i < count; ++i) if (symbols[i] >= 64) return Fail("Invalid password symbol");
    Interp816 cpu; PasswordBus *bus = Stage(ram,&cpu); if (!bus) return false;
    memset(bus->ram + 0xe2d0,0xff,60); memcpy(bus->ram + 0xe2d0,symbols,count);
    bool ok = Call(bus,&cpu,0x86cb27); cpu.a = (uint16_t)count;
    if (ok) ok = Call(bus,&cpu,0x83f674);
    if (ok && (cpu.c || !CampaignFlags(bus->ram))) ok = Fail("Cartridge rejected this campaign password");
    if (ok && decoded_ram) memcpy(decoded_ram,bus->ram,sizeof(bus->ram));
    free(bus); return ok;
}

bool issd_password_input_ready(const uint8_t *ram) {
    return s_rom && ram && Word(ram,0x32) == 6 && Word(ram,0x70) == 12 &&
        Pointer(ram,0x1446) == 0x8aea20 && Pointer(ram,0x1538) == 0x8aea20 &&
        !Word(ram,0x154e) && !Word(ram,0x1460) && !Word(ram,0x1462);
}

bool issd_password_submit(uint8_t *ram, const uint8_t *symbols, size_t count) {
    s_error = "";
    if (!s_rom) return Fail(s_context_error);
    if (!issd_password_input_ready(ram)) return Fail("Open the original Password screen before importing");
    if (!issd_password_decode(symbols,count,ram,NULL)) return false;
    /* Queue the exact $8A:EAAD callback normally reached by selecting OK.
     * Neutral input returns early in $8A:EA20, so changing mode2 alone cannot
     * submit. $1446 is the active task callback mirror; $1538 is its saved task
     * continuation. Both must describe the queued original guest routine.
     * The $1640.. campaign block is intentionally untouched here. */
    memset(ram + 0xe2d0,0xff,60); memcpy(ram + 0xe2d0,symbols,count);
    PutWord(ram,0x1542,(unsigned)count); PutWord(ram,0x1546,(unsigned)count);
    PutWord(ram,0x1548,2); PutWord(ram,0x1544,0x47);
    PutWord(ram,0x1538,0xeaad); ram[0x153a] = 0x8a;
    PutWord(ram,0x1446,0xeaad); ram[0x1448] = 0x8a;
    return true;
}
