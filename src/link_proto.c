/* Game Boy Color two-device link packet and Ready state. No transport wait,
 * malloc, VBlank drawing, campaign saves or other port assumptions. */
#if !defined(__SDCC) || defined(TEST_LINK)
#include "link_proto.h"
#include <string.h>
static uint8_t gbl_crc(const uint8_t *p,uint8_t n){
    uint8_t crc=0;
    while(n--){
        uint8_t k;
        crc^=*p++;
        for(k=0;k<8;k++)crc=(uint8_t)((crc<<1)^((crc&128u)?7u:0u));
    }
    return crc;
}
int8_t gbc_link_pack(uint8_t out[GBL_BYTES],const GbcLinkPacket *p){
    uint8_t i;
    if(!out || !p || p->kind<GBL_HELLO || p->kind>GBL_BYE ||
       (p->flags&GBL_FLAG_MODE)<GBL_MODE_DUEL ||
       (p->flags&GBL_FLAG_MODE)>GBL_MODE_COOP)return -1;
    out[0]=GBL_MAGIC;out[1]=GBL_VERSION;out[2]=p->kind;out[3]=p->seq;
    out[4]=(uint8_t)p->tick;out[5]=(uint8_t)(p->tick>>8);
    out[6]=p->flags;
    for(i=0;i<4;i++)out[7+i]=(uint8_t)(p->seed>>(i*8));
    out[11]=gbl_crc(out,11);
    return 0;
}
int8_t gbc_link_unpack(GbcLinkPacket *p,const uint8_t in[GBL_BYTES]){
    GbcLinkPacket next;
    uint8_t i;
    if(!p || !in)return -1;
    if(in[0]!=GBL_MAGIC || in[1]!=GBL_VERSION)return -2;
    if(in[2]<GBL_HELLO || in[2]>GBL_BYE ||
       (in[6]&GBL_FLAG_MODE)<GBL_MODE_DUEL ||
       (in[6]&GBL_FLAG_MODE)>GBL_MODE_COOP)return -3;
    if(gbl_crc(in,11)!=in[11])return -4;
    next.kind=in[2];next.seq=in[3];
    next.tick=(uint16_t)in[4]|((uint16_t)in[5]<<8);
    next.flags=in[6];next.seed=0;
    for(i=0;i<4;i++)next.seed|=(uint32_t)in[7+i]<<(i*8);
    *p=next;return 0; /* malformed bytes never poison session */
}
void gbc_link_init(GbcLinkSession *s,uint8_t role,uint8_t mode,uint32_t seed){
    if(!s)return;
    memset(s,0,sizeof *s);
    if(role<1 || role>2 || mode<GBL_MODE_DUEL || mode>GBL_MODE_COOP)return;
    s->role=role;s->mode=mode;s->seed=seed;s->local_active=1;
}
void gbc_link_set_ready(GbcLinkSession *s,uint8_t ready){
    if(s && s->role)s->local_ready=(uint8_t)(ready!=0);
}
void gbc_link_set_active(GbcLinkSession *s,uint8_t active){
    if(s && s->role)s->local_active=(uint8_t)(active!=0);
}
static void gbl_reset_peer(GbcLinkSession *s){
    s->connected=0;s->peer_ready=0;s->peer_active=0;
    s->local_ready=0;s->seen_seq=0;s->reconnecting=1;
}
void gbc_link_timeout(GbcLinkSession *s,uint16_t tick){
    if(s && s->role && s->connected &&
       (uint16_t)(tick-s->last_rx_tick)>GBL_TIMEOUT_TICKS)gbl_reset_peer(s);
}
void gbc_link_make(GbcLinkSession *s,GbcLinkPacket *p,uint16_t tick){
    if(!s || !p || !s->role)return;
    p->seed=s->seed;p->tick=tick;p->seq=s->tx_seq++;
    p->kind=s->connected?GBL_READY:GBL_HELLO;
    p->flags=(uint8_t)(s->mode |
         (s->local_active?GBL_FLAG_ACTIVE:0u) |
         (s->local_ready?GBL_FLAG_READY:0u));
}
int8_t gbc_link_receive(GbcLinkSession *s,const GbcLinkPacket *p,uint16_t now){
    uint8_t seq;
    if(!s || !p || !s->role || p->seed!=s->seed ||
       (p->flags&GBL_FLAG_MODE)!=s->mode)return -1;
    if(p->kind<GBL_HELLO || p->kind>GBL_BYE)return -2;
    if(s->seen_seq){
        seq=(uint8_t)(p->seq-s->peer_seq);
        if(!seq || seq>127u)return -3; /* wrap-aware anti-replay */
    }
    s->seen_seq=1;s->peer_seq=p->seq;
    s->last_rx_tick=now;
    if(p->kind==GBL_BYE){
        gbl_reset_peer(s);return 1;
    }
    if(p->kind==GBL_HELLO){
        if(!s->connected){
            s->connected=1;s->peer_ready=0;s->peer_active=0;
        }
        return 1;
    }
    if(!s->connected)return 0; /* require a HELLO before READY/INPUT */
    s->peer_ready=(uint8_t)((p->flags&GBL_FLAG_READY)!=0u);
    s->peer_active=(uint8_t)((p->flags&GBL_FLAG_ACTIVE)!=0u);
    if(gbc_link_can_play(s))s->reconnecting=0;
    return 1;
}
uint8_t gbc_link_both_confirmed(const GbcLinkSession *s){
    return (uint8_t)(s && s->connected && s->local_ready && s->peer_ready);
}
uint8_t gbc_link_can_play(const GbcLinkSession *s){
    return (uint8_t)(gbc_link_both_confirmed(s) &&
                     s->local_active && s->peer_active);
}

#endif /* TEST_LINK on cartridge; always available for host unit tests */
