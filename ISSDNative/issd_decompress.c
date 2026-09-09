#include "issd_decompress.h"
#include "common_rtl.h"
#include "snes/snes.h"
#include "snes/ppu.h"
#include "cpu_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_SIZE 0x400
#define WINDOW_MASK 0x3FF
#define MAX_DECOMP_BUFFER 0x10000

bool ISSD_Decompress(const uint8_t *src, size_t src_len,
                     uint8_t *dst, size_t dst_max,
                     size_t *out_decomp_len, bool *out_interleaved) {
    if (!src || src_len < 2 || !dst || dst_max == 0) {
        return false;
    }

    uint16_t comp_size_raw = (uint16_t)src[0] | ((uint16_t)src[1] << 8);
    uint16_t comp_size = comp_size_raw & 0x7FFF;
    bool is_interleaved = (comp_size_raw & 0x8000) != 0;

    if (out_interleaved) {
        *out_interleaved = is_interleaved;
    }

    if (comp_size <= 2 || comp_size > src_len) {
        return false;
    }

    uint8_t win_buf[WINDOW_SIZE];
    memset(win_buf, 0, sizeof(win_buf));

    size_t in_pos = 2;
    size_t out_pos = 0;
    uint16_t buf_pos = 0;

    while (in_pos < comp_size && out_pos < dst_max) {
        uint8_t b = src[in_pos++];
        uint8_t ctrl = b >> 5;

        switch (ctrl) {
            case 4: { /* RAW (0x80 - 0x9F) */
                uint16_t cnt = (uint16_t)(b & 0x1F);
                for (uint16_t i = 0; i < cnt; i++) {
                    if (in_pos >= comp_size || out_pos >= dst_max) break;
                    uint8_t val = src[in_pos++];
                    dst[out_pos++] = val;
                    win_buf[buf_pos] = val;
                    buf_pos = (buf_pos + 1) & WINDOW_MASK;
                }
                break;
            }

            case 5: { /* RLE_A0 (0xA0 - 0xBF) interleaved 0x00 and byte */
                uint16_t cnt = (uint16_t)(b & 0x1F) + 2;
                for (uint16_t i = 0; i < cnt; i++) {
                    if (in_pos >= comp_size || out_pos + 1 >= dst_max) break;
                    uint8_t val = src[in_pos++];
                    win_buf[buf_pos] = 0;
                    buf_pos = (buf_pos + 1) & WINDOW_MASK;
                    dst[out_pos++] = 0;

                    win_buf[buf_pos] = val;
                    buf_pos = (buf_pos + 1) & WINDOW_MASK;
                    dst[out_pos++] = val;
                }
                break;
            }

            case 6: { /* RLE_C0 (0xC0 - 0xDF) repeated byte */
                uint16_t cnt = (uint16_t)(b & 0x1F) + 2;
                if (in_pos >= comp_size) break;
                uint8_t val = src[in_pos++];
                for (uint16_t i = 0; i < cnt; i++) {
                    if (out_pos >= dst_max) break;
                    dst[out_pos++] = val;
                    win_buf[buf_pos] = val;
                    buf_pos = (buf_pos + 1) & WINDOW_MASK;
                }
                break;
            }

            case 7: { /* RLE_E0 (0xE0 - 0xFF) zero fill */
                uint16_t cnt;
                if (b != 0xFF) {
                    cnt = (uint16_t)(b & 0x1F) + 2;
                } else {
                    if (in_pos >= comp_size) break;
                    cnt = (uint16_t)src[in_pos++] + 2;
                }
                for (uint16_t i = 0; i < cnt; i++) {
                    if (out_pos >= dst_max) break;
                    dst[out_pos++] = 0;
                    win_buf[buf_pos] = 0;
                    buf_pos = (buf_pos + 1) & WINDOW_MASK;
                }
                break;
            }

            default: { /* LZ (0x00 - 0x7F) dictionary back-reference */
                uint8_t lz1 = b;
                if (in_pos >= comp_size) break;
                uint8_t lz2 = src[in_pos++];
                uint16_t lz_len = (uint16_t)(lz1 >> 2) + 2;
                uint16_t lz_off = (uint16_t)(((lz1 & 0x03) << 8) | lz2);
                lz_off = (lz_off - 0x3DF) & WINDOW_MASK;

                for (uint16_t i = 0; i < lz_len; i++) {
                    if (out_pos >= dst_max) break;
                    uint8_t val = win_buf[lz_off];
                    win_buf[buf_pos] = val;
                    buf_pos = (buf_pos + 1) & WINDOW_MASK;
                    dst[out_pos++] = val;
                    lz_off = (lz_off + 1) & WINDOW_MASK;
                }
                break;
            }
        }
    }

    if (out_decomp_len) {
        *out_decomp_len = out_pos;
    }
    return true;
}

static void ISSD_DeinterleaveTiles(uint8_t *buf, size_t len) {
    for (size_t t = 0; t + 16 <= len; t += 16) {
        uint8_t tmp[16];
        memcpy(tmp, &buf[t], 16);
        for (int j = 0; j < 8; j++) {
            buf[t + j * 2]     = tmp[j];
            buf[t + j * 2 + 1] = tmp[j + 8];
        }
    }
}

bool ISSD_ProcessDescriptor(CpuState *cpu, uint32_t descriptor_addr24) {
    if (!cpu) return false;

    uint8_t type = *RomPtr(descriptor_addr24);
    uint32_t curr = descriptor_addr24 + 2;

    static uint8_t s_decomp_scratch[MAX_DECOMP_BUFFER];

    if (type == 0x00) {
        /* Type 0: VRAM word destination */
        while (1) {
            uint8_t term = *RomPtr(curr);
            if (term == 0xFF) {
                break; /* Terminator byte */
            }

            uint16_t vram_word = (uint16_t)*RomPtr(curr) | ((uint16_t)*RomPtr(curr + 1) << 8);
            uint32_t raw_src_ptr = (uint32_t)*RomPtr(curr + 2) |
                                   ((uint32_t)*RomPtr(curr + 3) << 8) |
                                   ((uint32_t)*RomPtr(curr + 4) << 16);
            curr += 5;

            uint32_t clean_src_ptr = (raw_src_ptr & 0xBF0000) | (raw_src_ptr & 0x00FFFF);

            size_t decomp_len = 0;
            bool interleaved = false;
            if (ISSD_Decompress(RomPtr(clean_src_ptr), MAX_DECOMP_BUFFER,
                                s_decomp_scratch, sizeof(s_decomp_scratch),
                                &decomp_len, &interleaved)) {
                if (interleaved) {
                    ISSD_DeinterleaveTiles(s_decomp_scratch, decomp_len);
                }

                extern Snes *g_snes;
                Ppu *ppu = g_snes ? g_snes->ppu : NULL;
                if (ppu) {
                    uint8_t flags = *RomPtr(descriptor_addr24 + 1);
                    uint8_t vram_mode = (flags >> 1) & 0x03;

                    if (vram_mode == 0) {
                        /* Normal word write: 2 bytes per VRAM word */
                        size_t word_count = decomp_len / 2;
                        for (size_t w = 0; w < word_count; w++) {
                            uint16_t val = (uint16_t)s_decomp_scratch[w * 2] |
                                           ((uint16_t)s_decomp_scratch[w * 2 + 1] << 8);
                            ppu->vram[(vram_word + w) & 0x7FFF] = val;
                        }
                    } else if (vram_mode == 1 || vram_mode == 2) {
                        /* Low byte write only ($2118 PortLo) */
                        for (size_t w = 0; w < decomp_len; w++) {
                            uint16_t orig = ppu->vram[(vram_word + w) & 0x7FFF];
                            ppu->vram[(vram_word + w) & 0x7FFF] = (orig & 0xFF00) | (uint16_t)s_decomp_scratch[w];
                        }
                    } else if (vram_mode == 3) {
                        /* High byte write only ($2119 PortHi) */
                        for (size_t w = 0; w < decomp_len; w++) {
                            uint16_t orig = ppu->vram[(vram_word + w) & 0x7FFF];
                            ppu->vram[(vram_word + w) & 0x7FFF] = (orig & 0x00FF) | ((uint16_t)s_decomp_scratch[w] << 8);
                        }
                    }
                }
            }
        }

        /* Direct Page and queue cleanup */
        g_ram[0x0100] = 0;
        g_ram[0x0101] = 0;
        g_ram[0x1F00] = 0;
        g_ram[0x1F01] = 0;
        g_ram[0x0130] = 0;
        g_ram[0x0131] = 0;

        /* Normalize guest CPU mode: REP #$30 -> 16-bit M and X */
        cpu->m_flag = 0;
        cpu->x_flag = 0;
        cpu->P &= ~0x30;
        cpu->A = 0;
        cpu->_flag_Z = 1;
        cpu->_flag_N = 0;
        cpu->P = (uint8_t)((cpu->P & ~0x82) | 0x02);

        return true;
    } else if (type == 0x01) {
        /* Type 1: WRAM 24-bit destination */
        while (1) {
            uint16_t term = (uint16_t)*RomPtr(curr) | ((uint16_t)*RomPtr(curr + 1) << 8);
            if (term == 0xFFFF) {
                break; /* Terminator word */
            }

            uint32_t dst_wram = (uint32_t)*RomPtr(curr) |
                                ((uint32_t)*RomPtr(curr + 1) << 8) |
                                ((uint32_t)*RomPtr(curr + 2) << 16);
            uint32_t raw_src_ptr = (uint32_t)*RomPtr(curr + 3) |
                                  ((uint32_t)*RomPtr(curr + 4) << 8) |
                                  ((uint32_t)*RomPtr(curr + 5) << 16);
            curr += 6;

            bool is_map32 = (raw_src_ptr & 0x400000) != 0;
            uint32_t clean_src_ptr = (raw_src_ptr & 0xBF0000) | (raw_src_ptr & 0x00FFFF);

            size_t decomp_len = 0;
            bool interleaved = false;
            if (ISSD_Decompress(RomPtr(clean_src_ptr), MAX_DECOMP_BUFFER,
                                s_decomp_scratch, sizeof(s_decomp_scratch),
                                &decomp_len, &interleaved)) {
                uint8_t bank = (uint8_t)(dst_wram >> 16);
                uint16_t addr = (uint16_t)dst_wram;
                size_t wram_off = (bank == 0x7F) ? (0x10000 + addr) : (addr & 0xFFFF);

                if (is_map32) {
                    /* Stride-2 interleaved tilemap write (Map32) */
                    for (size_t i = 0; i < decomp_len; i++) {
                        if (wram_off + i * 2 < sizeof(g_ram)) {
                            g_ram[wram_off + i * 2] = s_decomp_scratch[i];
                        }
                    }
                } else {
                    if (interleaved) {
                        ISSD_DeinterleaveTiles(s_decomp_scratch, decomp_len);
                    }
                    if (wram_off + decomp_len <= sizeof(g_ram)) {
                        memcpy(&g_ram[wram_off], s_decomp_scratch, decomp_len);
                    }
                }
            }
        }

        /* Direct Page and queue cleanup */
        g_ram[0x0100] = 0;
        g_ram[0x0101] = 0;
        g_ram[0x1F00] = 0;
        g_ram[0x1F01] = 0;
        g_ram[0x0130] = 0;
        g_ram[0x0131] = 0;

        /* Normalize guest CPU mode: REP #$30 -> 16-bit M and X */
        cpu->m_flag = 0;
        cpu->x_flag = 0;
        cpu->P &= ~0x30;
        cpu->A = 0;
        cpu->_flag_Z = 1;
        cpu->_flag_N = 0;
        cpu->P = (uint8_t)((cpu->P & ~0x82) | 0x02);

        return true;
    }

    /* Unhandled types (e.g. Type 2 APU audio upload) fall back to original 65816 logic */
    return false;
}

bool Issd_HleDecompress(CpuState *cpu) {
    if (!cpu) return false;
    uint32_t desc_addr = 0x820000 | (uint32_t)(cpu->X & 0xFFFF);
    return ISSD_ProcessDescriptor(cpu, desc_addr);
}
