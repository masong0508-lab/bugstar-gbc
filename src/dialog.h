#ifndef DIALOG_H
#define DIALOG_H
#include <gb/gb.h>
/* Typewriter dialogue box (step 4c). A 20x6 framed box in the window layer that slides up from the bottom,
   over the HUD if the HUD is showing (the HUD slides back in afterwards).
   Text word-wraps to 18 columns, '\n' forces a line break, and long text pages: 3 lines per page with a speaker
   name, 4 without. A or B at the blinking arrow turns the page; A or B while typing rushes the rest of that page. */
#define DLG_ROWS 6
#define DLG_Y    (144 - DLG_ROWS * 8)
#define DLG_W    18                        /* text columns */

void  dlg_say(const char *name, const char *text);   /* blocking; name may be 0 for no speaker line */
void  dlg_set_speed(UINT8 frames_per_char);          /* default 2; 0 = instant */
void  dlg_set_blip(void (*fn)(void));                /* called for each visible character typed (sound hook), or 0 */
/* the word-wrapper, exposed for tests: copies the next line of *pp into out (NUL-terminated), returns its length,
   or 255 when the text is used up. */
UINT8 dlg_next_line(const char **pp, char *out);
#endif
