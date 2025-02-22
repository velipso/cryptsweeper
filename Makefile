#
# cryptsweeper - fight through the crypt to slay the vampire
# by Sean Connelly (@velipso), https://sean.fun
# Project Home: https://github.com/velipso/cryptsweeper
# SPDX-License-Identifier: 0BSD
#

NAME := cryptsweeper

PREFIX   := arm-none-eabi-
CC       := $(PREFIX)gcc
LD       := $(PREFIX)ld
OBJDUMP  := $(PREFIX)objdump
OBJCOPY  := $(PREFIX)objcopy
MKDIR    := mkdir
RM       := rm -rf
SYS      := sys
SRC      := src
DATA     := data
SND      := $(DATA)/snd
SCR      := $(DATA)/screens
TGT      := tgt
TGT_DATA := $(TGT)/$(DATA)
TGT_SND  := $(TGT)/$(SND)

ELF  := $(TGT)/$(NAME).elf
DUMP := $(TGT)/$(NAME).dump
ROM  := $(TGT)/$(NAME).gba
MAP  := $(TGT)/$(NAME).map

XFORM := $(TGT)/xform/xform

SOURCES_S := $(wildcard $(SRC)/*.s $(SRC)/**/*.s $(SYS)/*.s $(SYS)/gba/*.s $(SYS)/gba/**/*.s)
SOURCES_C := $(wildcard $(SRC)/*.c $(SRC)/**/*.c $(SYS)/*.c $(SYS)/gba/*.c $(SYS)/gba/**/*.c)
SOURCES_WAV := $(wildcard $(SND)/*.wav)
SOURCES_SCR := $(wildcard $(SCR)/*.png)

DEFINES := -DSYS_GBA
DEFINES += -D__GBA__
DEFINES += -DSYS_PRINT

LIBS     := -lc
INCLUDES := $(SYS)

ARCH := -mcpu=arm7tdmi -mtune=arm7tdmi

WARNFLAGS := -Wall

INCLUDEFLAGS := $(foreach path,$(INCLUDES),-I$(path))

ASFLAGS :=
ASFLAGS += \
	-x assembler-with-cpp $(DEFINES) $(ARCH) \
	-mthumb -mthumb-interwork $(INCLUDEFLAGS) \
	-ffunction-sections -fdata-sections

CFLAGS += \
	-std=gnu11 $(WARNFLAGS) $(DEFINES) $(ARCH) \
	-mthumb -mthumb-interwork $(INCLUDEFLAGS) -O3 \
	-ffunction-sections -fdata-sections

LDFLAGS := \
	-mthumb -mthumb-interwork \
	-Wl,-Map,$(MAP) -Wl,--gc-sections \
	-specs=nano.specs -T $(SYS)/gba/link.ld \
	-Wl,--start-group $(LIBS) -Wl,--end-group

OBJS := \
	$(patsubst %.s,$(TGT)/%.s.o,$(SOURCES_S)) \
	$(patsubst %.c,$(TGT)/%.c.o,$(SOURCES_C)) \
	$(patsubst %.png,$(TGT)/%.o,$(SOURCES_SCR)) \
	$(TGT_DATA)/tiles.o \
	$(TGT_DATA)/ui.o \
	$(TGT_DATA)/sprites.o \
	$(TGT_DATA)/popups.o \
	$(TGT_DATA)/palette_brightness.o \
	$(TGT_DATA)/levels.o \
	$(TGT_DATA)/song1.o \
	$(TGT_SND)/snd_osc.o \
	$(TGT_SND)/snd_tempo.o \
	$(TGT_SND)/snd_slice.o \
	$(TGT_SND)/snd_dphase.o \
	$(TGT_SND)/snd_bend.o \
	$(TGT_SND)/snd_wavs.o \
	$(TGT_SND)/snd_offsets.o \
	$(TGT_SND)/snd_sizes.o \
	$(TGT_SND)/snd_names.o

DEPS := $(OBJS:.o=.d)

# verifies binary files are divisible by 4 -- apparently the linker script just ignores alignment
# for binary blobs!??
verifyfilealign = \
	filesize=$$(wc -c $(1) | awk '{print $$1}'); \
	if [ $$((filesize % 4)) -ne 0 ]; then \
		echo "Error: File size of $(1) is not divisible by 4"; \
		exit 1; \
	fi

# converts a binary file to an object file in the ROM
#   input.bin -> input.o
#     extern const u8 _binary_input_bin_start[];
#     extern const u8 _binary_input_bin_end[];
#     extern const u8 _binary_input_bin_size[];
objbinary = $(call verifyfilealign,$1) ;\
	cd $(dir $1) ;\
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm \
	--rename-section .data=.rodata,alloc,load,readonly,data,contents \
	$(notdir $1) $(basename $(notdir $1)).o

$(TGT)/%.s.o: %.s
	$(MKDIR) -p $(@D)
	$(CC) $(ASFLAGS) -I$(dir $<) -MMD -MP -c -o $@ $<

$(TGT)/%.c.o: %.c
	$(MKDIR) -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(TGT)/%.o: %.png $(XFORM) $(TGT_DATA)/palette.bin
	$(MKDIR) -p $(@D)
	$(XFORM) copy256 $< $(TGT_DATA)/palette.bin $(subst .bin,.png,$@)
	$(call objbinary,$(subst .bin,.png,$@))

.PHONY: all clean dump

all: $(ROM)

$(TGT_DATA)/palette.bin: \
	$(DATA)/tiles.png \
	$(DATA)/ui.png \
	$(DATA)/sprites.png \
	$(DATA)/popups.png \
	$(SOURCES_SCR) \
	$(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) palette256 $(TGT_DATA)/palette.bin \
		$(DATA)/tiles.png $(DATA)/ui.png $(DATA)/sprites.png $(DATA)/popups.png $(SOURCES_SCR)

$(TGT_DATA)/palette_brightness.o: $(TGT_DATA)/palette.bin
	$(XFORM) brightness $(TGT_DATA)/palette_brightness.bin \
		$(TGT_DATA)/palette.bin 10 2 2.2 2.3 12 23
	$(call objbinary,$(TGT_DATA)/palette_brightness.bin)

$(TGT_DATA)/tiles.o: $(DATA)/tiles.png $(TGT_DATA)/palette.bin $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) copy8x8 $(DATA)/tiles.png $(TGT_DATA)/palette.bin $(TGT_DATA)/tiles.bin
	$(call objbinary,$(TGT_DATA)/tiles.bin)

$(TGT_DATA)/ui.o: $(DATA)/ui.png $(TGT_DATA)/palette.bin $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) copy8x8 $(DATA)/ui.png $(TGT_DATA)/palette.bin $(TGT_DATA)/ui.bin
	$(call objbinary,$(TGT_DATA)/ui.bin)

$(TGT_DATA)/sprites.o: $(DATA)/sprites.png $(TGT_DATA)/palette.bin $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) copy8x8 $(DATA)/sprites.png $(TGT_DATA)/palette.bin $(TGT_DATA)/sprites.bin
	$(call objbinary,$(TGT_DATA)/sprites.bin)

$(TGT_DATA)/popups.o: $(DATA)/popups.png $(TGT_DATA)/palette.bin $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) copy8x8 $(DATA)/popups.png $(TGT_DATA)/palette.bin $(TGT_DATA)/popups.bin
	$(call objbinary,$(TGT_DATA)/popups.bin)

$(TGT_DATA)/levels.o: $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) levels 1024 123 $(TGT_DATA)/levels.bin
	$(call objbinary,$(TGT_DATA)/levels.bin)

$(TGT_SND)/snd_osc.o \
$(TGT_SND)/snd_tempo.o \
$(TGT_SND)/snd_slice.o \
$(TGT_SND)/snd_dphase.o \
$(TGT_SND)/snd_bend.o: $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) snd tables \
		$(TGT_SND)/snd_osc.bin \
		$(TGT_SND)/snd_tempo.bin \
		$(TGT_SND)/snd_slice.bin \
		$(TGT_SND)/snd_dphase.bin \
		$(TGT_SND)/snd_bend.bin
	$(call objbinary,$(TGT_SND)/snd_osc.bin)
	$(call objbinary,$(TGT_SND)/snd_tempo.bin)
	$(call objbinary,$(TGT_SND)/snd_slice.bin)
	$(call objbinary,$(TGT_SND)/snd_dphase.bin)
	$(call objbinary,$(TGT_SND)/snd_bend.bin)

$(TGT_SND)/snd_wavs.o \
$(TGT_SND)/snd_offsets.o \
$(TGT_SND)/snd_sizes.o \
$(TGT_SND)/snd_names.o \
$(TGT_SND)/snd_names.txt: $(SOURCES_WAV) $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) snd wav $(SND) \
		$(TGT_SND)/snd_wavs.bin \
		$(TGT_SND)/snd_offsets.bin \
		$(TGT_SND)/snd_sizes.bin \
		$(TGT_SND)/snd_names.txt
	$(call objbinary,$(TGT_SND)/snd_wavs.bin)
	$(call objbinary,$(TGT_SND)/snd_offsets.bin)
	$(call objbinary,$(TGT_SND)/snd_sizes.bin)
	$(call objbinary,$(TGT_SND)/snd_names.txt)

$(TGT_DATA)/song1.o: $(DATA)/song1.txt $(TGT_SND)/snd_names.txt $(XFORM)
	$(MKDIR) -p $(@D)
	$(XFORM) snd makesong $(DATA)/song1.txt $(TGT_SND)/snd_names.txt $(TGT_DATA)/song1.gvsong
	$(call objbinary,$(TGT_DATA)/song1.gvsong)

$(XFORM):
	cd xform && make

$(ELF): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(ROM): $(ELF) $(XFORM)
	$(OBJCOPY) -O binary $< $@
	$(XFORM) fix $@

$(DUMP): $(ELF)
	$(OBJDUMP) -h -C -S $< > $@

dump: $(DUMP)

clean:
	$(RM) $(TGT)

-include $(DEPS)
