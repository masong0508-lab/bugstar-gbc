/* Game Boy Color 2-system serial transport. Byte transfer is interrupt
 * driven; the main loop advances one byte once per VBlank, never spins.
 * No allocation, SRAM writes, new framebuffers, or banked calls in the ISR.
 * Master: internal fast clock; guest: externally clocked fast serial.
 * Do not enter normal UI until linked emulator/device tests pass. */
#ifdef TEST_LINK
#include <gb/gb.h>
#include <gb/cgb.h>
#include "link_cable.h"

static GbcLinkSession link_state;
static uint8_t tx[GBL_BYTES],rx[GBL_BYTES];
static uint8_t tx_i,rx_n,link_on,link_role,old_sio_enable;
static uint16_t last_start_tick;
static volatile uint8_t sio_busy,sio_ready,sio_byte;

static void cable_irq(void){
    sio_byte=SB_REG;
    sio_ready=1;
    sio_busy=0;
}
static void prepare(uint16_t tick){
    GbcLinkPacket p;
    gbc_link_make(&link_state,&p,tick);
    (void)gbc_link_pack(tx,&p);
    tx_i=0;
}
/* Scrape for a fresh frame boundary if either emulator drops a byte.
 * A malformed packet must never overwrite session state. */
static void consume(uint8_t b,uint16_t tick){
    uint8_t i,n;
    GbcLinkPacket p;
    if(rx_n<GBL_BYTES)rx[rx_n++]=b;
    if(rx_n!=GBL_BYTES)return;
    if(gbc_link_unpack(&p,rx)==0){
        (void)gbc_link_receive(&link_state,&p,tick);
        rx_n=0;return;
    }
    for(i=1;i<GBL_BYTES;i++)if(rx[i]==GBL_MAGIC)break;
    n=(uint8_t)(GBL_BYTES-i);
    for(i=0;i<n;i++)rx[i]=rx[GBL_BYTES-n+i];
    rx_n=n;
}
void gbc_cable_start(uint8_t role,uint8_t mode,uint32_t seed){
    if(link_on)return;
    if(role!=1 && role!=2)return;
    gbc_link_init(&link_state,role,mode,seed);
    if(!link_state.role)return;
    link_role=role;link_on=1;tx_i=rx_n=0;
    sio_busy=sio_ready=0;last_start_tick=0;
    prepare(0);
    old_sio_enable=IE_REG&SIO_IFLAG;
    add_SIO(cable_irq);
    set_interrupts((uint8_t)(IE_REG|SIO_IFLAG));
    /* Guest must be ready BEFORE the master clocks the first byte.
     * If start is late, the rolling MAGIC/CRC parser resynchronizes. */
    if(role==2){
        SB_REG=tx[0];
        SC_REG=SIOF_XFER_START|SIOF_SPEED_32X|SIOF_CLOCK_EXT;
        sio_busy=1;
    }
}
void gbc_cable_step(uint16_t tick){
    uint8_t b=0,got=0; /* always initialized before IRQ handoff */
    if(!link_on)return;
    gbc_link_timeout(&link_state,tick);
    /* Atomic handoff: the ISR only stores one byte and two flags. */
    __critical {
        if(sio_ready){
            b=sio_byte;sio_ready=0;got=1;
        }
    }
    if(got){
        consume(b,tick);
        if(++tx_i==GBL_BYTES)prepare(tick);
    }
    /* No cable / external clock: re-arm instead of freezing input/music. */
    if(sio_busy && (uint16_t)(tick-last_start_tick)>5u){
        sio_busy=0;rx_n=0;prepare(tick);SC_REG=0;
    }
    if(!sio_busy){
        SB_REG=tx[tx_i];
        SC_REG=SIOF_XFER_START|SIOF_SPEED_32X|
                (link_role==1?SIOF_CLOCK_INT:SIOF_CLOCK_EXT);
        sio_busy=1;last_start_tick=tick;
    }
}
void gbc_cable_ready(uint8_t ready){
    if(link_on)gbc_link_set_ready(&link_state,ready);
}
const GbcLinkSession *gbc_cable_session(void){
    return link_on?&link_state:0;
}
void gbc_cable_stop(void){
    if(!link_on)return;
    SC_REG=0;
    if(!old_sio_enable)set_interrupts((uint8_t)(IE_REG&~SIO_IFLAG));
    remove_SIO(cable_irq);
    link_on=0;sio_ready=sio_busy=0;rx_n=tx_i=0;
    gbc_link_init(&link_state,0,0,0);
}

#endif /* TEST_LINK: the retail/solo ROM has no cable or ISR overhead */
