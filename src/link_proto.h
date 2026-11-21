#ifndef BUGSTAR_GBC_LINK_PROTO_H
#define BUGSTAR_GBC_LINK_PROTO_H
/* Small Game Boy Color link semantics: NOT PC UDP/GBA SIO wire bytes.
 * Fixed-size, zero-heap, no save writes or graphics side effects.
 * Every field serialized explicitly; 12 bytes/packet (no C struct memcpy).
 */
#include <stdint.h>
#define GBL_MAGIC 0xB7u
#define GBL_VERSION 1u
#define GBL_BYTES 12u
#define GBL_TIMEOUT_TICKS 90u
#define GBL_MODE_DUEL 1u
#define GBL_MODE_COOP 2u
#define GBL_FLAG_MODE 0x03u
#define GBL_FLAG_ACTIVE 0x40u
#define GBL_FLAG_READY 0x80u
enum { GBL_HELLO=1, GBL_READY=2, GBL_INPUT=3, GBL_BYE=4 };
typedef struct GbcLinkPacket {
    uint32_t seed;
    uint16_t tick;
    uint8_t kind,seq,flags;
} GbcLinkPacket;
typedef struct GbcLinkSession {
    uint32_t seed;
    uint16_t last_rx_tick;
    uint8_t role,mode,tx_seq,peer_seq,seen_seq;
    uint8_t connected,local_ready,peer_ready,local_active,peer_active,reconnecting;
} GbcLinkSession;
int8_t gbc_link_pack(uint8_t out[GBL_BYTES],const GbcLinkPacket *p);
int8_t gbc_link_unpack(GbcLinkPacket *p,const uint8_t in[GBL_BYTES]);
void gbc_link_init(GbcLinkSession *s,uint8_t role,uint8_t mode,uint32_t seed);
void gbc_link_set_ready(GbcLinkSession *s,uint8_t ready);
void gbc_link_set_active(GbcLinkSession *s,uint8_t active);
void gbc_link_timeout(GbcLinkSession *s,uint16_t tick);
void gbc_link_make(GbcLinkSession *s,GbcLinkPacket *p,uint16_t tick);
int8_t gbc_link_receive(GbcLinkSession *s,const GbcLinkPacket *p,uint16_t now);
uint8_t gbc_link_both_confirmed(const GbcLinkSession *s);
uint8_t gbc_link_can_play(const GbcLinkSession *s);
#endif
