#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Call on boot/reset and after any successful snapshot restoration. RAM is
 * the complete 128 KiB guest WRAM; healthy must describe the guest frame before
 * any host stack/NMI repair. A non-NULL static label requests one checkpoint. */
void issd_campaign_reset(void);
const char *issd_campaign_tick(const uint8_t *ram, bool healthy);
/* Pure scene eligibility; callers must additionally require a healthy frame. */
bool issd_campaign_can_export_password(const uint8_t *ram);
/* Call only after validated submission to the cartridge password task. */
void issd_campaign_note_password_import(void);
