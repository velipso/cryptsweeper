#
# cryptsweeper - fight the graveyard monsters and stop death
# by Pocket Pulp (@velipso), https://pulp.biz
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
SOURCES_C := $(wildcard $(SRC)/*.c $(SRC)/**/*.c) $(TGT)/game.c $(TGT)/rnd.c
LDFLAGS   := -lm
OBJS      := $(patsubst $(SRC)/%.c,$(TGT)/%.c.o,$(SOURCES_C))
DEPS      := $(OBJS:.o=.d)

$(TGT)/%.c.o : $(SRC)/%.c
	$(MKDIR) -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

.PHONY: all clean

all: $(TGT)/$(NAME)

$(TGT)/game.c \
$(TGT)/game.h \
$(TGT)/rnd.c \
$(TGT)/rnd.h: ./../src/game.c ./../src/game.h ./../src/rnd.c ./../src/rnd.h
	$(MKDIR) -p $(@D)
	cp ./../src/game.c ./../src/game.h $(@D)
	cp ./../src/rnd.c ./../src/rnd.h $(@D)

$(TGT)/$(NAME): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) $(TGT)

-include $(DEPS)
