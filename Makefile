# BUGSTAR GBC - GBDK-2020. Build: make   (needs GBDK_HOME, e.g. /opt/gbdk)
GBDK_HOME ?= /opt/gbdk
LCC = $(GBDK_HOME)/bin/lcc
# -Wm-yC: GBC only. -Wm-yt0x1B: MBC5+RAM+BATTERY. -Wm-yo16: 16 ROM banks (256K, grow later). -Wm-ya4: 32K SRAM
FLAGS = -Wm-yC -Wm-yt0x1B -Wm-yo16 -Wm-ya4 -Wm-yn"BUGSTAR"
SRCS = $(wildcard src/*.c)
all: bugstar.gbc
bugstar.gbc: $(SRCS) $(wildcard src/*.h)
	$(LCC) $(FLAGS) -Wl-m -o $@ $(SRCS)
	python3 tools/check_banks.py bugstar.map
clean:
	rm -f host_test bugstar.gbc *.map *.sym *.noi *.lst *.ihx *.o *.asm
# Regenerate the title art (needs python3 + numpy + pillow); the generated src/title_art.* are committed.
# Regenerate the bug part art (src/bugparts.c, tools/bugparts_check.png); needs python3 (+ pillow for the PNG).
bug:
	python3 tools/make_bug.py
art:
	python3 tools/make_title.py && python3 tools/img2gbc.py
# Regenerate the font + UI tiles (src/font.c, src/font.h, tools/font_check.png); needs python3 (+ pillow for the PNG).
font:
	python3 tools/make_font.py
# Runs the save/menu logic on the PC against a fake SRAM (needs only gcc).
test:
	gcc -std=gnu99 -O0 -w -fno-builtin -Itests/stub -Isrc '-DSRAM=(sram_buf+stub_bank*8192)' '-DRAM_ON()=' '-DRAM_OFF()=' '-DRAM_BANK(b)=(stub_bank=(b))' -include tests/pre.h tests/host_test.c src/input.c src/save.c src/menus.c src/util.c src/text.c src/font.c src/hud.c src/dialog.c src/sysmenu.c src/bug.c src/bugparts.c -o host_test && ./host_test > /dev/null && echo "host tests: ALL OK"
.PHONY: all clean art bug font test
