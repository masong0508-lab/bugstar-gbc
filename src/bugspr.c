#include <gb/gb.h>
#include <gb/cgb.h>
#include "bugspr.h"

void bugspr_colors(const BugLook *l){
    UINT16 p[16];
    bug_palettes(l, p);
    set_sprite_palette(BUGSPR_PAL0, 4, (palette_color_t *)p);
}
void bugspr_load(const BugLook *l){
    UINT8 f;
    bug_build(l);
    SPRITES_8x16;
    VBK_REG = VBK_BANK_0;
    for(f = 0; f < BUG_FRAMES; f++)
        set_sprite_data(BUGSPR_TILE0 + f * BUG_MAXCELL * 2, BUG_MAXCELL * 2, bug_set.tiles[f][0]);
    bugspr_colors(l);
}
UINT8 bugspr_w(void){ return bug_set.w; }
void bugspr_show(UINT8 x, UINT8 y, UINT8 flip, UINT8 frame){
    UINT8 i, nb;
    for(i = 0; i < bug_set.n; i++){
        const BugSpr *s = &bug_set.s[i];
        UINT8 sx = flip ? x + bug_set.w - 8 - s->x : x + s->x;
        nb = BUGSPR_OAM0 + i;
        set_sprite_tile(nb, BUGSPR_TILE0 + (frame * BUG_MAXCELL + s->cell) * 2);
        set_sprite_prop(nb, (BUGSPR_PAL0 + s->pal) | (flip ? S_FLIPX : 0));
        move_sprite(nb, sx + 8, y + s->y + 16 - (frame & 1));
    }
    for(; i < BUG_MAXCELL; i++) move_sprite(BUGSPR_OAM0 + i, 0, 0);
}
void bugspr_hide(void){
    UINT8 i;
    for(i = 0; i < BUG_MAXCELL; i++) move_sprite(BUGSPR_OAM0 + i, 0, 0);
    SPRITES_8x8;
}
