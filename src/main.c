/* BUGSTAR GBC - step 2: boot + input + title art. See PLAN.md */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdio.h>
#include "input.h"
#include "title.h"

void main(void){
    if(_cpu != CGB_TYPE) { printf("NEEDS GAME BOY COLOR"); return; }
    cpu_fast();                       /* double speed: 8 MHz */
    DISPLAY_ON;
    for(;;){
        title_init();
        int cont = title_run();
        title_exit();
        gotoxy(2, 8); printf(cont ? "CONTINUE: TODO" : "NEW GAME: TODO");
        waitpad(J_START); waitpadup();
        cls();
    }
}
