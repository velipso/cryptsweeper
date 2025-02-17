#
# cryptsweeper - fight through the crypt to slay the vampire
# by Sean Connelly (@velipso), https://sean.fun
# Project Home: https://github.com/velipso/cryptsweeper
# SPDX-License-Identifier: 0BSD
#

NAME      := xform
SRC       := .
TGT       := ./../tgt/xform
CC        := gcc
MKDIR     := mkdir
RM        := rm -rf
CFLAGS    := -Wall -O3
SOURCES_C := $(wildcard $(SRC)/*.c $(SRC)/**/*.c) $(TGT)/levelgen.c
LDFLAGS   := -lm
OBJS      := $(patsubst $(SRC)/%.c,$(TGT)/%.c.o,$(SOURCES_C))
DEPS      := $(OBJS:.o=.d)

$(TGT)/%.c.o : $(SRC)/%.c
	$(MKDIR) -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

.PHONY: all clean

all: $(TGT)/$(NAME)

$(TGT)/levelgen.c \
$(TGT)/levelgen.h: ./../src/levelgen.c ./../src/levelgen.h
	$(MKDIR) -p $(@D)
	cp ./../src/levelgen.c ./../src/levelgen.h $(@D)

$(TGT)/$(NAME): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) $(TGT)

-include $(DEPS)
