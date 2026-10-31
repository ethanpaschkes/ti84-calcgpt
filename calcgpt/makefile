# ----------------------------
# Makefile Options
# ----------------------------

NAME = CALCGPT
ICON = icon.png
DESCRIPTION = "CalcGPT chat"
COMPRESSED = YES

# code shared with CALC (../shared): the USB link, the Wi-Fi setup screen, the typing layer and the drawing helpers;
# and the Math app's cards (../math/src): the form, the solvers behind the diagrams, the drawing of ChatGPT's tool cards
SHARED = ../shared
MATH = ../math/src
EXTRA_CSOURCES = $(SHARED)/link.c $(SHARED)/outbuf.c $(SHARED)/wifi_screen.c $(SHARED)/wifi_draw.c $(SHARED)/draw.c $(SHARED)/typing.c \
                 $(SHARED)/timefmt.c $(MATH)/num.c $(MATH)/tri.c $(MATH)/probs.c $(MATH)/form.c $(MATH)/cards.c
CFLAGS = -Wall -Wextra -Oz -I$(SHARED) -I$(MATH)
CXXFLAGS = -Wall -Wextra -Oz
LTOFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
