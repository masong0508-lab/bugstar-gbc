#!/usr/bin/env python3
"""Run the actual GBC queen-victory dialogue with all eight memory cores.

The first queen must NOT reveal the ship's identity. The last queen must
pay off the name. This extracts the real feat.c dialogue and the actual
world_freed / text helpers into a sanitizer-equipped host fixture, verifying
that all bank-local text is first copied to its existing 110-byte WRAM area.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
feat=(ROOT/"src/feat.c").read_text()
save=(ROOT/"src/save.c").read_text()
util=(ROOT/"src/util.c").read_text()
bolt=(ROOT/"src/bolt_data.h").read_text()

def function(source, marker):
    m=re.search(re.escape(marker)+r"[^;{}]*\{",source)
    assert m,marker
    start=source.index("{",m.start());n=1;end=start+1
    while n:
        n+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[m.start():end]

def table(source,mark):
    i=source.index(mark)
    j=source.index("{",i);depth,end=1,j+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[i:end]+";"

mem_table=table(feat,"static const char *const memory[8]")
tricks=table(feat,"static const char *const trick_nm[12]")
queen=function(feat,"void dungeon_won(")
freed=function(save,"UINT8 world_freed(")
scat=function(util,"char *scat(")
u16s=function(util,"char *u16s(")
strings=re.findall(r'"((?:\\.|[^"\\])*)"',mem_table)
assert len(strings)==8
assert all(0<len(s)<110 for s in strings)
assert all("DAWN" not in s for s in strings[:7]), "a queen reveals the final name too soon"
assert "MY NAME IS DAWN." in strings[7], "eighth memory must deliver the reveal"
assert "A NAME WAS PAINTED ON MY HULL." in strings[0]
assert "THE KING CUT OPEN MY HEART." in strings[4]
assert "I ESCAPED IN THIS LITTLE TIN BODY." in strings[5]
assert "THE KING NEVER MADE ME SAY" in strings[6]
assert 'dlg_say("BOLT", memory[sv.cores - 1]);' not in queen
assert "b[0]=0; scat(b,memory[sv.cores-1]);" in queen
assert 'dlg_say("BOLT",b);' in queen
assert "if(world_freed(sv.star))" in queen
assert "if(sv.cores >= 8)" in queen
assert "sv.freed[sv.cores++] = sv.star;" in queen

# Contrast with the companion's eight memories: all pre-reveal clues agree.
genmem=re.search(r"static const char \*const bolt_mem\[32\]\s*=\s*\{(.*?)\};",bolt,re.S)
assert genmem and len(re.findall(r'"(?:\\.|[^"\\])*"',genmem.group(1)))==32
assert "THE DAWN" not in genmem.group(1).split('"MY NAME HAS COME BACK AT LAST."')[0]

stub=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define BANKED
#define WRAM_TXT 0
static char wram[110];
#define WRAM_AT(offset,type) ((type *)wram)
typedef struct {UINT8 star,zone,cores,freed[8];} Save;
static Save sv;
static struct {UINT16 tricks;} ex;
#define TRICK(kind) ((ex.tricks>>(kind))&1u)
#define SFX_LEVEL 3
static int effects, dialog_count, overflow;
static char spoken[8][110];
static void sfx(int n){(void)n;effects++;}
static void dlg_say(const char *speaker,const char *text){
    size_t n=strlen(text);
    if(strcmp(speaker,"BOLT") || n>=sizeof wram) overflow++;
    if(dialog_count<8){
        memcpy(spoken[dialog_count],text,n>=sizeof wram?sizeof wram-1:n);
        spoken[dialog_count][n>=sizeof wram?sizeof wram-1:n]=0;
    }
    dialog_count++;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Queen memory test line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
cases=r"""
static void clear_dialogue(void){
    dialog_count=0; overflow=0; effects=0;
    memset(spoken,0,sizeof spoken);
    memset(wram,0,sizeof wram);
}
int main(void){
    memset(&sv,0,sizeof sv);
    for(int i=0;i<8;i++){
        clear_dialogue(); sv.star=(UINT8)(40+i);
        CHECK(!world_freed(sv.star));
        dungeon_won(12); // no bonus-trick dialogue, just queen and memory
        CHECK(sv.cores==i+1 && sv.freed[i]==sv.star);
        CHECK(sv.zone==i+1 && dialog_count==3 && !overflow);
        CHECK(strcmp(spoken[1],memory[i])==0);
        CHECK(strstr(spoken[1],"DAWN")==(i==7?strstr(spoken[1],"DAWN"):NULL));
        if(i<7)CHECK(!strstr(spoken[1],"DAWN"));
        else {CHECK(strstr(spoken[1],"MY NAME IS DAWN")); CHECK(strstr(spoken[2],"GALAXY CHART")); CHECK(strstr(spoken[2],"CORE GLOWS"));}
        CHECK(effects==1);
    }
    // Re-fighting a queen does not pay a second memory or reset any core.
    clear_dialogue();sv.star=43;dungeon_won(12);
    CHECK(sv.cores==8 && sv.zone==8 && dialog_count==1);
    CHECK(strstr(spoken[0],"CAME BACK") && !overflow);

    // The post-eight-core generic victory must not read memory[8].
    clear_dialogue();sv.star=100;dungeon_won(12);
    CHECK(sv.cores==8 && sv.zone==9 && dialog_count==1 && !overflow);

    // Every 0..255 star ID and every prior core count must choose the
    // correct memory with a destination safe for the GBC dialogue buffer.
    int cases=0;
    for(int s=0;s<256;s++)for(int old=0;old<8;old++){
        memset(&sv,0,sizeof sv);memset(&ex,0,sizeof ex);
        sv.star=(UINT8)s;sv.cores=(UINT8)old;
        for(int i=0;i<old;i++)sv.freed[i]=(UINT8)((s+i+1)&255);
        clear_dialogue();dungeon_won(12);
        CHECK(sv.cores==old+1 && sv.freed[old]==s && dialog_count==3 && !overflow);
        CHECK(!strcmp(spoken[1],memory[old]));
        cases++;
    }
    printf("GBC queen memories: 8 chronological clues, replay/no-duplicate, %d world/core victories and bank-safe text PASS\n",cases);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="gbc-queen-story-") as temp:
    src=Path(temp)/"queen.c";exe=Path(temp)/"queen"
    src.write_text(stub+"\n"+mem_table+"\n"+tricks+"\n"+scat+
                   "\n"+u16s+"\n"+freed+"\n"+queen+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                    "-fno-omit-frame-pointer",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
