/* Physical/emulator link handshake diagnostic; not an advertised
 * multiplayer mode. Two ROM instances select A=clock host, B=guest.
 * Check connected/Ready/Active, cable unplug and leave without saving. */
#include <gb/gb.h>
#include "link_test.h"
#include "link_cable.h"
#include "input.h"
#include "text.h"
static void show(const char *msg,uint8_t row){
    txt_line(1,row,msg,18);
}
void gbc_link_test_run(void){
    uint8_t role;
    uint16_t tick;
    for(;;){
        role=0;tick=0;
        txt_cls();
        show("GBC LINK TEST",1);
        show("A: MASTER CLOCK",4);
        show("B: CABLE GUEST",6);
        show("2 LINKED SYSTEMS",9);
        do{
            wait_vbl_done();input_poll();
            if(HIT(J_A))role=1;
            else if(HIT(J_B))role=2;
        }while(!role);
        gbc_cable_start(role,GBL_MODE_COOP,0xBEEFA11u);
        txt_cls();
        show(role==1?"P1 MASTER":"P2 GUEST",1);
        show("A: READY",12);
        show("B: LEAVE",14);
        for(;;){
            const GbcLinkSession *s;
            wait_vbl_done();input_poll();tick++;
            if(HIT(J_B))break;
            s=gbc_cable_session();
            if(HIT(J_A) && s)gbc_cable_ready(!s->local_ready);
            gbc_cable_step(tick);
            s=gbc_cable_session();
            if(s && (tick%12u)==0){
                show(s->connected?"LINKED":"WAIT LINK...",4);
                show(s->local_ready?"YOU READY":"YOU NOT READY",6);
                show(s->peer_ready?"PEER READY":"PEER WAITING",8);
                show(gbc_link_can_play(s)?"BOTH READY":"NO MATCH START",10);
            }
        }
        gbc_cable_stop();
    }
}
