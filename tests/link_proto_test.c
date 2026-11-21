/* Real GBC wire codec and two-peer Ready state, portable host test.
 * Does NOT claim that a real link cable transport or full game is done. */
#include "../src/link_proto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void deliver(GbcLinkSession *a,GbcLinkSession *b,uint16_t tick){
    GbcLinkPacket send,read;
    uint8_t bytes[GBL_BYTES];
    gbc_link_make(a,&send,tick);
    assert(!gbc_link_pack(bytes,&send));
    assert(!gbc_link_unpack(&read,bytes));
    assert(gbc_link_receive(b,&read,tick)==1);
}
int main(void){
    GbcLinkPacket a,b={0},old;
    uint8_t wire[GBL_BYTES],bad[GBL_BYTES];
    for(uint32_t t=0;t<10000;t++){
        a.seed=0x2468ACE1u^t;a.tick=(uint16_t)t;
        a.kind=(uint8_t)(GBL_HELLO+t%4u);
        a.seq=(uint8_t)t;
        a.flags=GBL_MODE_COOP|(t%2?GBL_FLAG_READY:0u);
        assert(!gbc_link_pack(wire,&a));
        assert(!gbc_link_unpack(&b,wire));
        assert(b.seed==a.seed && b.tick==a.tick && b.flags==a.flags &&
               b.kind==a.kind && b.seq==a.seq);
        old=b;memcpy(bad,wire,sizeof bad);
        bad[7]^=1u;
        assert(gbc_link_unpack(&b,bad)==-4);
        assert(memcmp(&old,&b,sizeof b)==0);
        bad[0]=0;
        assert(gbc_link_unpack(&b,bad)==-2);
        memcpy(bad,wire,sizeof bad);bad[1]=0;
        assert(gbc_link_unpack(&b,bad)==-2);
    }
    GbcLinkSession host,guest;
    gbc_link_init(&host,1,GBL_MODE_COOP,0xBEEFu);
    gbc_link_init(&guest,2,GBL_MODE_COOP,0xBEEFu);
    deliver(&host,&guest,1);deliver(&guest,&host,2);
    assert(host.connected && guest.connected);
    assert(!gbc_link_both_confirmed(&host) && !gbc_link_can_play(&guest));
    gbc_link_set_ready(&host,1);
    deliver(&host,&guest,3);
    assert(!gbc_link_can_play(&host) && !gbc_link_can_play(&guest));
    gbc_link_set_ready(&guest,1);
    deliver(&guest,&host,4);
    assert(gbc_link_can_play(&host) && gbc_link_can_play(&guest));
    gbc_link_set_active(&host,0);deliver(&host,&guest,5);
    assert(!gbc_link_can_play(&host) && !gbc_link_can_play(&guest));
    gbc_link_set_active(&host,1);deliver(&host,&guest,6);
    assert(gbc_link_can_play(&host) && gbc_link_can_play(&guest));
    gbc_link_timeout(&guest,97);
    assert(!guest.connected && !guest.local_ready && guest.reconnecting);
    /* timeout leaves host's session intact, but reconnect resets readiness */
    gbc_link_timeout(&host,100);
    assert(!host.connected && !host.local_ready);
    deliver(&host,&guest,101);deliver(&guest,&host,102);
    assert(host.connected && guest.connected && !gbc_link_can_play(&host));
    gbc_link_set_ready(&host,1);gbc_link_set_ready(&guest,1);
    deliver(&host,&guest,103);deliver(&guest,&host,104);
    assert(gbc_link_can_play(&host) && gbc_link_can_play(&guest));
    old=b;old.seed=0xBADu;
    assert(gbc_link_receive(&guest,&old,105)==-1);
    assert(gbc_link_can_play(&guest));
    puts("GBC link codec: 10k frames, corruption/version/replay and 2-peer Ready/reconnect PASS");
    return 0;
}
