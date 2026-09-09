#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CpuState CpuState;

/**
 * Decompress a Konami LZSS/RLE compressed stream into a flat destination buffer.
 *
 * @param src              Pointer to the compressed data starting at the 2-byte header.
 * @param src_len          Maximum allowable bytes to read from src.
 * @param dst              Output buffer to receive decompressed bytes.
 * @param dst_max          Maximum capacity of dst.
 * @param out_decomp_len   Returns the number of decompressed bytes written.
 * @param out_interleaved  Returns true if bit 15 was set (tile deinterleaving flag).
 * @return                 true on successful decompression, false on corrupted/invalid data.
 */
bool ISSD_Decompress(const uint8_t *src, size_t src_len,
                     uint8_t *dst, size_t dst_max,
                     size_t *out_decomp_len, bool *out_interleaved);

/**
 * Process an ISSD decompression descriptor table in Bank $82 (or other banks).
 *
 * Supported descriptor types:
 *   - Type 0 (0x00): VRAM word destination. Directly writes words into PPU VRAM.
 *   - Type 1 (0x01): WRAM 24-bit destination. Directly writes bytes into g_ram.
 *
 * @param cpu                 Active guest CPU state.
 * @param descriptor_addr24   24-bit SNES address of the descriptor table.
 * @return                    true if handled by HLE, false if unhandled (fall back to LLE/AOT).
 */
bool ISSD_ProcessDescriptor(CpuState *cpu, uint32_t descriptor_addr24);

/**
 * Entry hook for CODE_80B527.
 * Called directly from CODE_80B527_M0X0 and CODE_80B527_M1X1.
 *
 * @param cpu   Active guest CPU state.
 * @return      true if decompression was fully handled by HLE, false to execute original code.
 */
bool Issd_HleDecompress(CpuState *cpu);

#ifdef __cplusplus
}
#endif
