# ----------------------------
# Makefile Options
# ----------------------------

NAME = MATH
ICON = icon.png
DESCRIPTION = "Guided math: triangles and more"
COMPRESSED = YES

# code shared with CALC and CALCGPT (../shared) and CALCGPT's message parser (../calcgpt/src/chatmodel.c)
SHARED = ../shared
EXTRA_CSOURCES = $(SHARED)/link.c $(SHARED)/outbuf.c $(SHARED)/draw.c $(SHARED)/typing.c $(SHARED)/timefmt.c ../calcgpt/src/chatmodel.c
CFLAGS = -Wall -Wextra -Oz -I$(SHARED) -I../calcgpt/src
CXXFLAGS = -Wall -Wextra -Oz
LTOFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
