# ----------------------------
# Makefile Options
# ----------------------------

NAME = CALCGPT
ICON = icon.png
DESCRIPTION = "CalcGPT chat"
COMPRESSED = NO

# code shared with CALC (../shared): the USB link, the Wi-Fi setup screen, the typing layer and the drawing helpers
SHARED = ../shared
EXTRA_CSOURCES = $(SHARED)/link.c $(SHARED)/outbuf.c $(SHARED)/wifi_screen.c $(SHARED)/wifi_draw.c $(SHARED)/draw.c $(SHARED)/typing.c
CFLAGS = -Wall -Wextra -Oz -I$(SHARED)
CXXFLAGS = -Wall -Wextra -Oz
LTOFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
