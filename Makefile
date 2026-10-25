# BUGSTAR GBC - GBDK-2020. Build: make   (needs GBDK_HOME, e.g. /opt/gbdk)
GBDK_HOME ?= /opt/gbdk
LCC = $(GBDK_HOME)/bin/lcc
# -Wm-yC: GBC only. -Wm-yt0x1B: MBC5+RAM+BATTERY. -Wm-yo512: 512 ROM banks = 8 MB, the most MBC5 can do. -Wm-ya4: 32K SRAM
# (GBDK banked calls reach banks 1..255; banks 256..511 are for data, switched with SWITCH_ROM_MBC5_8M)
# -Wl-g.STACK=0xD000: the stack tops out at D000, so all of WRAM D000-DFFF (7 banks) is free for big buffers (src/wram.h)
FLAGS = -Wm-yC -Wm-yt0x1B -Wm-yo512 -Wm-ya4 -Wm-yn"BUGSTAR" -Wl-g.STACK=0xD000
# Each .c compiles to its own object in build/ (make -j2 builds two at once, and only what changed is rebuilt).
# Test builds go to their own folder and ROM: make DEFS="-DTEST_SPACE" BUILD=build_test OUT=test.gbc
DEFS ?=
BUILD ?= build
OUT ?= bugstar.gbc
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)
HDRS = $(wildcard src/*.h)
all: $(OUT)
$(BUILD)/%.o: src/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(LCC) $(FLAGS) $(DEFS) -c -o $@ $<
$(OUT): $(OBJS)
	$(LCC) $(FLAGS) -Wl-m -o $@ $(OBJS)
	python3 tools/check_banks.py $(basename $(OUT)).map
clean:
	rm -rf build build_* host_test bugstar.gbc test*.gbc *.map *.sym *.noi *.lst *.ihx *.o *.asm
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
	gcc -std=gnu99 -O0 -w -fno-builtin -Itests/stub -Isrc '-DSRAM=(sram_buf+stub_bank*8192)' '-DRAM_ON()=' '-DRAM_OFF()=' '-DRAM_BANK(b)=(stub_bank=(b))' -include tests/pre.h tests/host_test.c src/input.c src/save.c src/econ.c src/menus.c src/util.c src/text.c src/font.c src/hud.c src/dialog.c src/sysmenu.c src/bug.c src/bugbuild.c src/bugparts.c -o host_test && ./host_test > /dev/null && echo "host tests: ALL OK"
.PHONY: all clean art bug font test
