# ----------------------------
# Makefile Options
# ----------------------------

NAME = CALC
ICON = icon.png
DESCRIPTION = "CALC home screen"
COMPRESSED = NO

# code shared with CALCGPT (../shared) and CALCGPT's message parser (../calcgpt/src/chatmodel.c)
SHARED = ../shared
EXTRA_CSOURCES = $(SHARED)/link.c $(SHARED)/outbuf.c $(SHARED)/wifi_screen.c $(SHARED)/wifi_draw.c $(SHARED)/draw.c \
                 $(SHARED)/typing.c $(SHARED)/timefmt.c ../calcgpt/src/chatmodel.c
CFLAGS = -Wall -Wextra -Oz -I$(SHARED) -I../calcgpt/src
CXXFLAGS = -Wall -Wextra -Oz
LTOFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
