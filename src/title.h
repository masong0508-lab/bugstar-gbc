#ifndef TITLE_H
#define TITLE_H
void title_init(void);
/* returns 0 = NEW GAME, 1 = CONTINUE */
int title_run(void);
/* hand the screen back to the printf text mode (font, palette, sprites) */
void title_exit(void);
#endif
