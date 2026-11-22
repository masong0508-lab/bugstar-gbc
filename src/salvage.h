#ifndef BUGSTAR_SPACE_SALVAGE_H
#define BUGSTAR_SPACE_SALVAGE_H
/* Same two pod slots in each view: no more sprites, WRAM or save fields.
   253/254 are supplies; all existing trade goods retain codes 0..7. */
#include "save.h"
#include "econ.h"
#include "hud.h"
#define SALV_FUEL 253
#define SALV_REPAIR 254
enum { SALV_GOODS, SALV_GAS, SALV_HP, SALV_MP, SALV_GOLD };

/* Return the kind of reward and its ACTUAL amount. A full tank/hull/hold
   pays gold rather than wasting a one-use pirate supply crate. */
static UINT8 salvage_take(UINT8 good, UINT8 n, UINT8 *got){
    UINT8 cap, room;
    if(good==SALV_FUEL){
        cap=TANK_CAP();
        if(sv.el.fuel<cap){
            room=cap-sv.el.fuel; *got=room<6?room:6;
            sv.el.fuel+=*got; return SALV_GAS;
        }
        *got=15; sv.gold+=15; return SALV_GOLD;
    }
    if(good==SALV_REPAIR){
        UINT16 mx=hud_hpmax(sv.level);
        if(sv.hp<mx){
            UINT16 deficit=mx-sv.hp;
            *got=deficit<12?(UINT8)deficit:12;
            sv.hp+=*got; return SALV_HP;
        }
        mx=hud_mpmax(sv.level);
        if(sv.mp<mx){
            UINT16 deficit=mx-sv.mp;
            *got=deficit<10?(UINT8)deficit:10;
            sv.mp+=*got; return SALV_MP;
        }
        *got=15; sv.gold+=15; return SALV_GOLD;
    }
    if(good<8){
        cap=HOLD_CAP(); room=cap>econ_load()?cap-econ_load():0;
        if(room){
            if(n>room) n=room;
            *got=n; sv.el.cargo[good]+=n; return SALV_GOODS;
        }
    }
    *got=10; sv.gold+=10; return SALV_GOLD;
}
#endif
