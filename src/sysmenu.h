#ifndef SYSMENU_H
#define SYSMENU_H
#include <gb/gb.h>
/* SYSTEM menu overlay (step 4d): a 12x8 box that slides in from the right edge in the window layer, over the HUD
   (the HUD comes back afterwards). RESUME / SAVE / OPTIONS / MIRROR / SKILLS / TO TITLE. B or START also resumes.
   SAVE writes the current slot and shows SAVED!. OPTIONS hides the overlay, runs the full-screen options page, then
   calls redraw() so the caller can repaint the scene behind the menu. TO TITLE asks first (autosaves if that option is on). */
#define SM_W   12
#define SM_H   13
#define SM_WX  (167 - SM_W * 8)            /* window X register value when fully in */
#define SM_WY  (144 - SM_H * 8)
extern UINT8 sys_gfx_lost;               /* 1 while redraw() must also reload the scene's tiles (after MIRROR) */
extern UINT8 sys_space;                  /* 1 while the caller is flying in space: EXTRAS may then switch the view or start the Arena */
enum { SYS_RESUME, SYS_TITLE, SYS_FLIP, SYS_ARENA, SYS_HELL, SYS_MOON, SYS_KART };   /* FLIP: the 2D / 3D view was switched; ARENA: start the Arena (only asked for when sys_space) */
int sysmenu_run(void (*redraw)(void));     /* returns SYS_RESUME or SYS_TITLE; redraw may be 0 */
#endif
