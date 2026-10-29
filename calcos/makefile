# ----------------------------
# Makefile Options
# ----------------------------

NAME = CALCOS
ICON = icon.png
DESCRIPTION = "CalcOS shell"
COMPRESSED = NO

# the typing layer and the clock text live in ../shared (also used by CALC and CALCGPT)
SHARED = ../shared
EXTRA_CSOURCES = $(SHARED)/typing.c $(SHARED)/timefmt.c
CFLAGS = -Wall -Wextra -Oz -I$(SHARED)
CXXFLAGS = -Wall -Wextra -Oz
LTOFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
