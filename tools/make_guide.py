#!/usr/bin/env python3
"""Makes src/guide_art.c (the little pictures of the how-to-play guide) and src/guide_pages.h (its text).
Each picture is an 8x8 drawing doubled to 16x16, in 3 colours + clear, cut into the 4 tiles of two 8x16 sprites (TL, BL, TR, BR)."""
import re
ICONS = {
'BUG':["..dddd..",".dRRRRd.","dRWRRWRd","dRRddRRd","dRWRRWRd",".dRRRRd.",".dg..gd.","........"],
'HEART':[".RR..RR.","RWRRRRRR","RRRRRRRR","RRRRRRRR",".RRRRRR.","..RRRR..","...RR...","........"],
'COIN':["..YYYY..",".YOOOOY.","YOYYYYOY","YOYOOYOY","YOYOOYOY","YOYYYYOY",".YOOOOY.","..YYYY.."],
'STAR':["...YY...","...YY...","YYYYYYYY",".YYYYYY.","..YYYY..",".YYYYYY.",".YY..YY.","........"],
'DOOR':["..OOOO..",".OOOOOO.","OOOOOOOO","OOOOOOOO","OOOOOYOO","OOOOOOOO","OOOOOOOO","dddddddd"],
'SHIP':["...CC...","..CWWC..","..CWWC..",".CCWWCC.","CCCCCCCC","CC.CC.CC","C..RR..C","...RR..."],
'SWORD':["......WW",".....WWg","....WWg.","...WWg..","Y.WWg...",".YWg....","..Y.....",".OY....."],
'SHELL':["CCCCCCCC","CWWCCCCC","CWCCCCCC","CCCCCCCC",".CCCCCC.","..CCCC..","...CC...","........"],
'POTION':["...gg...","...WW...","..WWWW..",".WRRRRW.",".WRRRRW.",".WRRRRW.","..WWWW..","........"],
'FISH':["........","..C..CC.",".CCCCCCC","CCYCCCC.","CCCCCCCC",".CCCCCC.","..C..CC.","........"],
'PLANT':["...GG...","..GGGG..",".GGGGGG.","G.GGGG.G","..GGGG..","...GG...","...GG...","..GGGG.."],
'GEM':["..CCCC..",".CWCCCC.","CWCCCCCC","CCCCCCCC",".CCCCCC.","..CCCC..","...CC...","........"],
'KEY':["..YYY...",".Y...Y..",".Y...Y..","..YYY...","...Y....","...YYY..","...Y....","...YY..."],
'MON':["..G..G..","..GGGG..",".GGGGGG.","GRGGGGRG","GGGGGGGG","GGWWWWGG",".GGGGGG.","G.G..G.G"],
'HOUSE':["...RR...","..RRRR..",".RRRRRR.","RRRRRRRR",".OOOOOO.",".OWOOWO.",".OOOYOO.",".OOOOOO."],
'FLAG':["gRRR....","gRRRR...","gRRRRR..","gRRRR...","gRRR....","g.......","g.......","gggg...."],
'BOOK':["CCCCCCC.","CYYYYYC.","CYCCCYC.","CYYYYYC.","CCCCCCC.","CWWWWWC.","CWWWWWC.",".CCCCC.."],
'FIRE':["...R....","..RR..R.","..RRR.RR",".RRORRRR",".ROOORRR","RROYOORR",".ROYYORR","..ROORR."],
'BOLT':["...C....","..CCC...","..gggg..",".gCCCCg.",".gCWWCg.",".gCCCCg.","..gggg..",".g.gg.g."],
'MOON':["..WWW...",".WW.....","WW......","WW......","WW......",".WW.....","..WWW...","........"],
'PLANET':["..GGGG..",".GCCGGG.","YYYYYYYY","GCCCGGGG","GGGGGCCG",".GGGGGG.","..GGGG..","........"],
'SKULL':[".WWWWWW.","WWWWWWWW","WddWWddW","WddWWddW","WWWddWWW",".WWWWWW.",".WdWdWW.","........"],
'CHEST':["........",".OOOOOO.","OOOOOOOO","YYYYYYYY","OOOOYOOO","OOOOOOOO","OOOOOOOO","dddddddd"],
'KART':["........","..RRRR..",".RWRRRR.","RRRRRRRR","RRRRRRRR",".dd..dd.","dddd.ddd",".dd..dd."],
}
RGB = {'W':(240,240,240),'R':(232,64,64),'D':(130,30,30),'G':(80,200,90),'Y':(250,210,70),'g':(160,160,172),'d':(76,76,104),'C':(90,200,240),'O':(240,140,40)}
names = list(ICONS)
def c555(c): return (c[0]>>3)|((c[1]>>3)<<5)|((c[2]>>3)<<10)
def near(a,b): return sum((x-y)**2 for x,y in zip(a,b))
tiles = bytearray(); pals = []
for n in names:
    rows = ICONS[n]; cnt = {}
    for r in rows:
        for ch in r:
            if ch != '.': cnt[ch] = cnt.get(ch,0)+1
    keep = [c for c,_ in sorted(cnt.items(), key=lambda kv:-kv[1])[:3]]
    pal = [0] + [c555(RGB[c]) for c in keep] + [0]*(3-len(keep))
    pals.append(pal[:4])
    def idx(ch):
        if ch == '.': return 0
        if ch in keep: return keep.index(ch)+1
        return 1+min(range(len(keep)), key=lambda i: near(RGB[ch], RGB[keep[i]]))
    big = [[idx(rows[y//2][x//2]) for x in range(16)] for y in range(16)]
    def tile(x0,y0):
        out = bytearray()
        for y in range(8):
            lo = hi = 0
            for x in range(8):
                v = big[y0+y][x0+x]; lo |= (v&1)<<(7-x); hi |= ((v>>1)&1)<<(7-x)
            out += bytes((lo,hi))
        return out
    tiles += tile(0,0) + tile(0,8) + tile(8,0) + tile(8,8)      # TL, BL, TR, BR
with open('src/guide_art.c','w') as f:
    f.write('#pragma bank 22\n#include <gb/gb.h>\n#include "guide.h"\n/* made by tools/make_guide.py */\n')
    f.write('const UINT8 guide_tiles[%d] = {%s};\n' % (len(tiles), ','.join(map(str,tiles))))
    f.write('const UINT16 guide_pals[%d] = {%s};\n' % (len(names)*4, ','.join(str(v) for p in pals for v in p)))
# ---- the pages
PAGES = [
("THE BASICS","BUG BOLT DOOR SHIP",["YOU ARE A PILL BUG.","BOLT IS YOUR DROID.","","EVERY WORLD HIDES A","DUNGEON BELOW A DOOR","BEAT THE QUEEN AT","THE BOTTOM TO FREE","THE WORLD. THEN FLY","TO THE NEXT STAR."]),
("BUTTONS","BUG DOOR SWORD SHIP",["D-PAD: WALK, STEER","A: TALK, ENTER, BITE","   FISH, USE","B: HOLD TO RUN, OR","   BOOST THE SHIP","START: THE MENU:","   SAVE, OPTIONS,","   SKILLS, FEATS","SELECT: SPELL, MAP"]),
("ON A PLANET","PLANET PLANT FISH DOOR",["WALK OVER THE LAND.","THE GOLD LINE AT THE","TOP SAYS WHAT A DOES","STAND NEXT TO WATER","AND PRESS A TO FISH.","CITIES HAVE GATES:","WALK IN AND PRESS A.","THE DOOR GOES DOWN","TO THE DUNGEON."]),
("CITIES","HOUSE COIN BOOK POTION",["3 CITIES PER WORLD,","FROM HAMLET TO","METROPOLIS.","INN: SLEEP TO DAWN","QUEST BOARD: JOBS","STORE, SHIPYARD,","ACADEMY: TRAIN","WORKSHOP: COOK FISH","BUG PIT: WAVES"]),
("DAY AND NIGHT","MOON STAR HOUSE POTION",["A DAY LASTS 8 MIN.","NIGHT IS DARK, WITH","FIREFLIES.","STORE, YARD, ACADEMY","AND WORKSHOP CLOSE.","CASINO AND FORTUNE","TELLER OPEN AT NIGHT","SLEEP AT THE INN TO","SKIP TO THE MORNING."]),
("IN A DUNGEON","DOOR MON CHEST BUG",["A: BITE WHERE YOU","   FACE. B: RUN.","BITE CHESTS OPEN.","POTIONS HEAL YOU.","STAIRS DOWN: DEEPER","UP STAIRS LEAVE.","THE LAST FLOOR HAS A","QUEEN AND CAGED BUGS","FAINT: LOSE GOLD"]),
("FLOOR EVENTS","CHEST COIN SKULL FIRE",["MOST FLOORS HAVE ONE","A MESSAGE TELLS YOU.","TREASURE VAULT:","  CHESTS EVERYWHERE","GOLD RUSH: 3X GOLD","AMBUSH: MORE FOES","BLESSED: FULL HEAL","CURSED: TOUGH FOES,","  DOUBLE GOLD"]),
("SPELLS","BOOK FIRE HEART SHELL",["SELECT: CAST SPELL","A+SELECT: NEXT SPELL","B+SELECT: POTION","SPARK (LV1): HURTS","  FOES NEAR YOU","FIREBALL LV3: A LINE","HEAL LV5, SHIELD LV8","MP REFILLS SLOWLY.","LEVEL UP: FULL MP."]),
("QUEENS","SKULL FIRE STAR BOLT",["EACH KIND OF WORLD","HAS A QUEEN BUG.","SHE CALLS HER BROOD.","BEAT HER FOR A TRICK","THAT STAYS FOREVER:","BIGGER BITE, TOUGHER","SHELL, FASTER RUN,","HEALING... AND BOLT","REMEMBERS THE PAST."]),
("FRIENDS","BUG FISH HEART POTION",["FREE CAGED BUGS.","UP TO 3 FOLLOW YOU","AND CHIP AT FOES.","MORE FRIENDS GIVE A","STRONGER BITE.","CATCH FISH AND COOK","THEM INTO POTIONS AT","A WORKSHOP, OR SELL","THEM. GOLDEN = LUCK."]),
("LEVELS AND SKILLS","STAR SWORD SHELL HEART",["KILLS GIVE XP.","LEVELS GIVE HP, MP","AND SKILL POINTS:","1 EACH, 3 EVERY 5TH.","START > SKILLS:","MIGHT, GRIT, GUARD,","LUCK, VIGOR.","SHOPS SELL FANG AND","SHELL UPGRADES."]),
("DIFFICULTY","HEART SHELL SKULL STAR",["START > OPTIONS >","DIFFICULTY:","BABY: 1 DAMAGE, 2X","  GOLD+XP, NO LOSS","EASY: HALF DAMAGE","NORMAL: AS IT COMES","HARD: 1.5X DAMAGE","BRUTAL: 2X DAMAGE","NIGHTMARE: 3X"]),
("IN SPACE","SHIP PLANET MOON SKULL",["D-PAD: STEER","B: BOOST  A: FIRE","NEAR A PLANET, A","LANDS. AT A STATION,","A DOCKS.","SELECT: GALAXY MAP.","PICK A STAR, JUMP.","JUMPS USE FUEL.","PIRATES SHOOT BACK."]),
("STATIONS","COIN GEM SHIP POTION",["BUY AND SELL GOODS:","PRICES CHANGE BY","WORLD. BUY LOW,","SELL HIGH.","REFUEL, REPAIR,","UPGRADE LASER AND","SHIELD. BIGGER HOLD","CARRIES MORE.","JOBS PAY WELL."]),
("THE GALAXY MAP","STAR PLANET FLAG SHIP",["STARS YOU LANDED ON","ARE MARKED. A CITY","MARK: YOU WALKED IN.","THE NAME LINE SAYS","SEEN OR CITY.","FAME UNLOCKS THE 3D","VIEW AND THE ARENA","(START > EXTRAS).",""]),
("YOUR QUEST","DOOR SKULL KEY BOLT",["1 LAND, FIND DOOR","2 BEAT THE QUEEN","3 FLY TO NEXT STAR","QUEENS GIVE BOLT A","CORE BACK. 8 CORES","RESTORE HIS NAME.","THEN FIND THE","TECHNOBEAST. SHOOT","ITS GLOWING CORE."]),
("TIPS","STAR HEART BOLT KART",["START > FEATS: GOALS","THAT PAY GOLD, FAME","MIRROR: REMAKE YOUR","BUG ANY TIME.","TITLE SCREEN: HOW TO","PLAY IS HERE.","STUCK? TRY EASY OR","BABY DIFFICULTY.","HAVE FUN!"]),
]
bad = [(t,l) for t,_,ls in PAGES for l in ls if len(l)>20]
for t,_,ls in PAGES:
    assert len(ls)==9,(t,len(ls))
    for l in ls: assert len(l)<=20,(t,l)
with open('src/guide_pages.h','w') as f:
    f.write('/* made by tools/make_guide.py: the text of the how-to-play pages. Lines are at most 20 letters. */\n#define GUIDE_N %d\n#define GUIDE_NICON %d\n' % (len(PAGES), len(names)))
    f.write('static const char *const gd_title[GUIDE_N] = {%s};\n' % ','.join('"%s"'%t for t,_,_ in PAGES))
    f.write('static const UINT8 gd_icon[GUIDE_N][4] = {%s};\n' % ','.join('{%s}'%','.join(str(names.index(x)) for x in i.split()) for _,i,_ in PAGES))
    f.write('static const char *const gd_line[GUIDE_N][9] = {\n%s};\n' % ',\n'.join('{%s}'%','.join('"%s"'%l for l in ls) for _,_,ls in PAGES))
print(len(PAGES),'pages',len(names),'icons','tiles',len(tiles)//16, [l for t,_,ls in PAGES for l in ls if len(l)>18])
