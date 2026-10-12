# BUGSTAR GBC - GBDK-2020. Build: make   (needs GBDK_HOME, e.g. /opt/gbdk)
GBDK_HOME ?= /opt/gbdk
LCC = $(GBDK_HOME)/bin/lcc
# -Wm-yC: GBC only. -Wm-yt0x1B: MBC5+RAM+BATTERY. -Wm-yo16: 16 ROM banks (256K, grow later). -Wm-ya4: 32K SRAM
FLAGS = -Wm-yC -Wm-yt0x1B -Wm-yo16 -Wm-ya4 -Wm-yn"BUGSTAR"
SRCS = $(wildcard src/*.c)
all: bugstar.gbc
bugstar.gbc: $(SRCS) $(wildcard src/*.h)
	$(LCC) $(FLAGS) -o $@ $(SRCS)
clean:
	rm -f bugstar.gbc *.map *.sym *.noi *.lst *.ihx *.o *.asm
# Regenerate the title art (needs python3 + numpy + pillow); the generated src/title_art.* are committed.
art:
	python3 tools/make_title.py && python3 tools/img2gbc.py
.PHONY: all clean art
