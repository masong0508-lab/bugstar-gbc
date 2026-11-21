/* Minimal two-console CGB link diagnostic: standalone ROM, never built into
 * the real Bugstar campaign. Compiles only the protocol, cable and font.
 * This prevents GBC bank-0 overflow and avoids touching title/IRQ/saves.
 * TILE == ASCII in the game's existing banked font; PyBoy reads tilemap.
 */
#ifdef TEST_LINK
#include <gb/gb.h>
#include <gb/cgb.h>
#include "font.h"
#include "link_cable.h"

static void diag_line(const char *s,uint8_t y){
    uint8_t row[18],i=0;
    for(;i<18 && s[i];i++)row[i]=(uint8_t)s[i];
    for(;i<18;i++)row[i]=0; /* exact text erasure after state changes */
    set_bkg_tiles(1,y,18,1,row);
}
static uint8_t diag_new_press(void){
    static uint8_t previous;
    uint8_t current=joypad();
    uint8_t pressed=(uint8_t)(current & (uint8_t)~previous);
    previous=current;
    return pressed;
}
void main(void){
    uint8_t keys,role=0;
    uint16_t tick;
    if(_cpu!=CGB_TYPE)for(;;)wait_vbl_done();
    cpu_fast();
    VBK_REG=VBK_BANK_0;
    bsfont_load(); /* font.c in ROM bank 7 */
    SHOW_BKG;DISPLAY_ON;
    for(;;){
        role=0;tick=0;
        diag_line("GBC LINK TEST",1);
        diag_line("A: MASTER CLOCK",4);
        diag_line("B: CABLE GUEST",6);
        diag_line("2 LINKED SYSTEMS",9);
        diag_line("",10);
        do{
            wait_vbl_done();keys=diag_new_press();
            if(keys&J_A)role=1;
            else if(keys&J_B)role=2;
        }while(!role);
        gbc_cable_start(role,GBL_MODE_COOP,0xBEEFA11u);
        diag_line(role==1?"P1 MASTER":"P2 GUEST",1);
        diag_line("A: READY",12);
        diag_line("B: LEAVE",14);
        for(;;){
            const GbcLinkSession *session;
            wait_vbl_done();keys=diag_new_press();tick++;
            if(keys&J_B)break;
            session=gbc_cable_session();
            if((keys&J_A) && session)gbc_cable_ready(!session->local_ready);
            gbc_cable_step(tick);
            session=gbc_cable_session();
            if(session && (tick%12u)==0){
                diag_line(session->connected?"LINKED":"WAIT LINK...",4);
                diag_line(session->local_ready?"YOU READY":"YOU NOT READY",6);
                diag_line(session->peer_ready?"PEER READY":"PEER WAITING",8);
                diag_line(gbc_link_can_play(session)?"BOTH READY":"NO MATCH START",10);
            }
        }
        gbc_cable_stop();
    }
}
#endif /* TEST_LINK */
