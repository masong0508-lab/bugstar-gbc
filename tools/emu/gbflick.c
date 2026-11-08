// gbrun.c: run a GB/GBC ROM headless (Walnut-CGB), press keys from a script, save screenshots / record frames.
// usage: gbrun ROM SCRIPT OUTPREFIX SAVEFILE. Script: "<start> <end> <keys>" (A B s S r l U D), "shot <frame>", "rec <start> <end> <step>"
#define ENABLE_SOUND 0
#define ENABLE_LCD 1
struct gb_s;
#include "walnut_cgb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct priv_t { uint8_t *rom; size_t romsz; uint8_t cart_ram[0x20000]; unsigned char fb[144][160][3]; };
static uint8_t snap[0x4000]; static long act, vbl;
#define MAPCNT(g) do{ for(int i_=0x1800;i_<0x2000;i_++){ if((g)->vram[i_]!=snap[i_]){ CNT++; snap[i_]=(g)->vram[i_]; } if((g)->vram[0x2000+i_]!=snap[0x2000+i_]){ CNT++; snap[0x2000+i_]=(g)->vram[0x2000+i_]; } } }while(0)
uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t a){ struct priv_t *p=gb->direct.priv; return a<p->romsz?p->rom[a]:0xFF; }
uint16_t gb_rom_read_16bit(struct gb_s *gb, const uint_fast32_t a){ return gb_rom_read(gb,a)|(gb_rom_read(gb,a+1)<<8); }
uint32_t gb_rom_read_32bit(struct gb_s *gb, const uint_fast32_t a){ return gb_rom_read_16bit(gb,a)|((uint32_t)gb_rom_read_16bit(gb,a+2)<<16); }
uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t a){ struct priv_t *p=gb->direct.priv; return p->cart_ram[a&0x1FFFF]; }
void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t a, const uint8_t v){ struct priv_t *p=gb->direct.priv; p->cart_ram[a&0x1FFFF]=v; }
void gb_error(struct gb_s *gb, const enum gb_error_e e, const uint16_t addr){ fprintf(stderr,"gb error %d at %04x\n",(int)e,addr); exit(2); }
void lcd_draw_line(struct gb_s *gb, const uint8_t px[160], const uint_fast8_t line){
    struct priv_t *p=gb->direct.priv;
#define CNT act
    MAPCNT(gb);
#undef CNT
    for(int x=0;x<160;x++){ uint16_t c;
        if(gb->cgb.cgbMode) c=gb->cgb.fixPalette[px[x]]; else { static const uint16_t g[4]={0xFFFF,0xAD55,0x52AA,0x0000}; c=g[px[x]&3]; }
        p->fb[line][x][0]=(unsigned char)(((c>>11)&31)*255/31); p->fb[line][x][1]=(unsigned char)(((c>>5)&63)*255/63); p->fb[line][x][2]=(unsigned char)((c&31)*255/31); } }
typedef struct { int a,b; unsigned k; } Ev; static Ev ev[16384]; static int nev, shots[8192], nshot, ra[256], rb[256], rs[256], nrec;
int main(int c, char **v){
    if(c<5){ fprintf(stderr,"usage\n"); return 1; }
    static struct priv_t pr; static struct gb_s gb;
    FILE *f=fopen(v[1],"rb"); if(!f){ fprintf(stderr,"no rom\n"); return 1; } fseek(f,0,SEEK_END); pr.romsz=ftell(f); rewind(f); pr.rom=malloc(pr.romsz); fread(pr.rom,1,pr.romsz,f); fclose(f);
    memset(pr.cart_ram,0xFF,sizeof pr.cart_ram); f=fopen(v[4],"rb"); if(f){ fread(pr.cart_ram,1,sizeof pr.cart_ram,f); fclose(f); }
    if(gb_init(&gb,&gb_rom_read,&gb_rom_read_16bit,&gb_rom_read_32bit,&gb_cart_ram_read,&gb_cart_ram_write,&gb_error,&pr)!=GB_INIT_NO_ERROR){ fprintf(stderr,"init fail\n"); return 1; }
    gb_init_lcd(&gb,&lcd_draw_line);
    f=fopen(v[2],"r"); char line[256]; int last=0;
    while(fgets(line,256,f)){ int a,b,s2; char ks[64];
        if(sscanf(line,"shot %d",&a)==1){ shots[nshot++]=a; if(a>last) last=a; continue; }
        if(sscanf(line,"rec %d %d %d",&a,&b,&s2)==3){ ra[nrec]=a; rb[nrec]=b; rs[nrec]=s2; nrec++; if(b>last) last=b; continue; }
        if(sscanf(line,"%d %d %63s",&a,&b,ks)==3){ unsigned k=0; for(char *p=ks;*p;p++) switch(*p){ case 'A':k|=JOYPAD_A;break; case 'B':k|=JOYPAD_B;break; case 's':k|=JOYPAD_SELECT;break; case 'S':k|=JOYPAD_START;break; case 'r':k|=JOYPAD_RIGHT;break; case 'l':k|=JOYPAD_LEFT;break; case 'U':k|=JOYPAD_UP;break; case 'D':k|=JOYPAD_DOWN;break; }
            ev[nev].a=a; ev[nev].b=b; ev[nev].k=k; nev++; if(b>last) last=b; } }
    fclose(f);
    char rn[512]; snprintf(rn,512,"%s.rgb",v[3]); FILE *ro = nrec ? fopen(rn,"wb") : 0;
    for(int fr=0;fr<=last;fr++){ unsigned k=0; for(int i=0;i<nev;i++) if(fr>=ev[i].a && fr<ev[i].b) k|=ev[i].k;
        static unsigned char prev[144][160][3]; static int have; long pix=0; act=0; vbl=0; gb.direct.joypad = 0xFF & ~k; gb_run_frame(&gb);
#define CNT vbl
        MAPCNT(&gb);
#undef CNT
        if(have){ for(int y_=0;y_<144;y_++) for(int x_=0;x_<160;x_++) if(memcmp(prev[y_][x_],pr.fb[y_][x_],3)) pix++; } memcpy(prev,pr.fb,sizeof prev); have=1;
        if(getenv("DUMPF") && fr==atoi(getenv("DUMPF"))){ FILE *d_=fopen(getenv("DUMPFILE")?getenv("DUMPFILE"):"dump.bin","wb"); fwrite(gb.vram,1,0x4000,d_); fwrite(gb.cgb.BGPalette,1,0x40,d_); fwrite(gb.cgb.OAMPalette,1,0x40,d_); uint8_t io_[8]={gb.hram_io[IO_LCDC],gb.hram_io[IO_SCX],gb.hram_io[IO_SCY],gb.hram_io[IO_WX],gb.hram_io[IO_WY],gb.hram_io[IO_LYC],gb.hram_io[IO_STAT],0}; fwrite(io_,1,8,d_); fclose(d_); }
        if(getenv("FLICK")) printf("f %d act %ld vbl %ld pix %ld\n",fr,act,vbl,pix);
        for(int i=0;i<nrec;i++) if(fr>=ra[i] && fr<rb[i] && (fr-ra[i])%rs[i]==0) fwrite(pr.fb,1,sizeof pr.fb,ro);
        for(int i=0;i<nshot;i++) if(shots[i]==fr){ char n[512]; snprintf(n,512,"%s_%06d.ppm",v[3],fr); FILE *o=fopen(n,"wb"); fprintf(o,"P6 160 144 255\n"); fwrite(pr.fb,1,sizeof pr.fb,o); fclose(o); } }
    if(ro) fclose(ro);
    f=fopen(v[4],"wb"); if(f){ fwrite(pr.cart_ram,1,0x8000,f); fclose(f); }
    return 0; }
