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
UINT8 salvage_take(UINT8 good, UINT8 n, UINT8 *got) BANKED;
#endif
