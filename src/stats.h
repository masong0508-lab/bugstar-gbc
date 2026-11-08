#ifndef STATS_H
#define STATS_H
#include <gb/gb.h>
/* STATS (bank 35): a book of numbers in EXTRAS, three pages (LEFT/RIGHT turn them): the adventure, the galaxy, the games. Read-only; it reads what the game already counts. */
void stats_run(void) BANKED;
#endif
