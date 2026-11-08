extern unsigned char sram_buf[]; extern int stub_bank;
/* host build stubs for things in banks the test does not link */
static void wild_load(void){} static void wild_save(void){} static void bounty_tick(void){}
