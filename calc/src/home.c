#include "home.h"
#include "keys.h"

const struct tile HOME_TILES[TILE_COUNT] = {
    {"Messages", ""},
    {"Settings", ""},
    {"ChatGPT", "CALCGPT"},
    {"Math", "SOLVER"},
};

int home_key(struct home *h, int key)
{
    if ((key == K_LEFT || key == K_UP) && h->sel > 0) h->sel--;
    else if ((key == K_RIGHT || key == K_DOWN) && h->sel < TILE_COUNT - 1) h->sel++;
    else if (key == K_ENTER) return HOME_OPEN;
    else if (key == K_CLEAR) return HOME_QUIT;
    return HOME_NONE;
}
