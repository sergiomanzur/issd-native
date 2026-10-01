#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "snes.h"
#include "joypad.h"

static uint16_t serial_word(Snes *snes, unsigned line) {
    uint16_t result = 0;
    for (int i = 0; i < 16; i++)
        result = (uint16_t)((result << 1) | ((joypad_read_serial(snes, 1) >> line) & 1));
    return result;
}

int main(void) {
    Snes snes;
    memset(&snes, 0, sizeof(snes));
    const uint16_t pads[4] = {1, 256, 512, 128};
    joypad_set_inputs(&snes, pads, 15, true);
    joypad_write_io(&snes, 255);
    joypad_write_strobe(&snes, 1);
    for (int i = 0; i < 8; i++) assert(joypad_read_serial(&snes, 1) == 2);
    joypad_write_strobe(&snes, 0);
    unsigned signature = 0;
    for (int i = 0; i < 8; i++) signature = (signature << 1) | (joypad_read_serial(&snes, 1) >> 1);
    assert(signature != 255); /* game's MULTI5 detection must recognize port two */

    joypad_write_strobe(&snes, 1);
    joypad_write_strobe(&snes, 0);
    joypad_write_io(&snes, 255);
    assert(serial_word(&snes, 0) == 0x0080); /* P2 A */
    assert(joypad_read_serial(&snes, 1) == 3);
    joypad_write_io(&snes, 127);
    assert(serial_word(&snes, 0) == 0x0100); /* P4 Right, independent pair counter */
    joypad_write_io(&snes, 255);
    assert(joypad_read_serial(&snes, 1) == 3);

    joypad_auto_poll(&snes);
    assert(joypad_read_auto(&snes, 0x4219) == 0x80); /* P1 B */
    assert(joypad_read_auto(&snes, 0x421a) == 0x80); /* P2 A */
    assert(joypad_read_auto(&snes, 0x421e) == 0x40); /* P3 X is JOY4, not JOY3 */
    assert(joypad_read_auto(&snes, 0x421c) == 0);
    assert(joypad_read_serial(&snes, 0) == 1);
    assert(joypad_read_serial(&snes, 1) == 3); /* auto poll advanced first pair */
    joypad_write_io(&snes, 127);
    assert(serial_word(&snes, 0) == 0x0100);

    joypad_set_inputs(&snes, pads, 5, true); /* P2 and P4 disconnected */
    joypad_auto_poll(&snes);
    assert(joypad_read_auto(&snes, 0x421a) == 255);
    joypad_write_io(&snes, 127);
    assert(serial_word(&snes, 0) == 0xffff); /* absent port cannot look like a pad */

    const uint16_t opposed[4] = {48, 192, 48, 192};
    joypad_set_inputs(&snes, opposed, 15, true);
    joypad_write_io(&snes, 255);
    joypad_auto_poll(&snes);
    assert(joypad_read_auto(&snes, 0x4219) == 0);
    assert(joypad_read_auto(&snes, 0x421b) == 0);
    assert(joypad_read_auto(&snes, 0x421f) == 0);
    joypad_write_io(&snes, 127);
    assert(serial_word(&snes, 0) == 0);

    /* Existing two-pad clients retain their bit order and presence behavior. */
    joypad_set_inputs(&snes, pads, 3, false);
    assert(joypad_read_auto(&snes, 0x421e) == 0);
    joypad_write_strobe(&snes, 1);
    assert(joypad_read_serial(&snes, 1) == 0);
    joypad_write_strobe(&snes, 0);
    assert(serial_word(&snes, 0) == 0x0080);
    puts("multitap: PASS");
    return 0;
}
