#ifndef BUGSTAR_GBC_LINK_CABLE_H
#define BUGSTAR_GBC_LINK_CABLE_H
/* GBC-only fast serial link: master provides clock, guest is externally
 * clocked. One byte per logical VBlank, no spin wait or framebuffer.
 * Experimental: real linked-emulator/hardware validation is still required.
 */
#include <stdint.h>
#include "link_proto.h"
void gbc_cable_start(uint8_t role,uint8_t mode,uint32_t seed);
void gbc_cable_stop(void);
void gbc_cable_step(uint16_t tick);
void gbc_cable_ready(uint8_t ready);
const GbcLinkSession *gbc_cable_session(void);
#endif
