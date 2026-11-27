#!/usr/bin/env python3
"""Test real station contract generation across the whole GBC galaxy."""
from pathlib import Path
import tempfile, subprocess
root=Path(__file__).resolve().parents[1]
src=(root/"src/econ.c").read_text()
elite=(root/"src/elite.c").read_text()
def function(mark):
    a=src.index(mark);b=src.index("{",a)+1;depth=1
    while depth:
        depth+=(src[b]=="{")-(src[b]=="}");b+=1
    return src[a:b]
prefix=r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;typedef int8_t INT8;typedef uint16_t UINT16;typedef uint32_t UINT32;
#define BANKED
enum{J_NONE,J_HUNT,J_SEND};
static struct{UINT8 zone;UINT32 gold;struct{UINT16 jobs,jrew;UINT8 jk,jn,jd,jp;} el;}sv;
static UINT8 fame_rank(void){return 2;}
static unsigned fame;
static void fame_add(UINT16 n){fame+=n;}
"""
cases=r"""
int main(void){
    unsigned checks=0;
    for(unsigned seed=0;seed<65536;seed+=1021)for(int z=0;z<6;z++)for(int star=0;star<256;star++){
        sv.zone=z;sv.el.jobs=seed;
        UINT8 kind[3],n[3],dest[3],k,m,d;UINT16 reward[3],p;
        for(int choice=0;choice<3;choice++){
            job_offer_pick(star,choice,&kind[choice],&n[choice],&dest[choice],&reward[choice]);
            job_offer_pick(star,choice,&k,&m,&d,&p);
            assert(k==kind[choice]&&m==n[choice]&&d==dest[choice]&&p==reward[choice]);
            if(choice==0)assert(k==J_HUNT&&m>=3&&m<=11&&p>0);
            else{
                assert(k==J_SEND&&m==0&&d!=star);
                UINT8 dist=cells_between(star,d);
                if(choice==1)assert(dist>=1&&dist<=3);
                else assert(dist>=5&&dist<=8);
                assert(!job_done(star));
            }
            checks++;
        }
        assert(dest[1]!=dest[2]&&reward[2]>reward[1]);
    }
    UINT8 k,n,d;UINT16 p;
    job_offer_pick(255,1,&k,&n,&d,&p);
    job_take(k,n,d,p);assert(!job_done(255)&&job_done(d));
    assert(job_pay()>p&&sv.el.jk==J_NONE&&!job_done(d));
    job_offer_pick(0,0,&k,&n,&d,&p);job_take(k,n,d,p);
    assert(!job_done(0));sv.el.jp=n;assert(job_done(123));job_pay();assert(!job_done(123));
    printf("Station contracts: %u deterministic offers, toroidal distance, fuel reachability and one-time rewards PASS\n",checks);
}
"""
assert "job_offer_pick(here, choice" in elite
assert 'msg="A AGAIN TO DROP"' in elite
assert 'if(job_done(here)){job_pay()' in elite
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"test.c";exe=Path(td)/"test"
    c.write_text(prefix+"\n".join(function(x) for x in ("UINT8 cells_between(","void job_offer_pick(","void job_take(","UINT8 job_done(","UINT16 job_pay("))+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-Werror","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
