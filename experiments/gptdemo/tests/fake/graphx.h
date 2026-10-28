#ifndef FAKE_GRAPHX_H
#define FAKE_GRAPHX_H
/* A stand-in for the calculator's graphics library that draws into an image on a computer, so the screens can be previewed.
 * Text is not drawn into the picture: each piece is written to a list so render_preview.py can draw it with a readable font
 * (the calculator's own font is not available here). */
#include <stdint.h>
extern uint16_t gfx_palette[256];
#define gfx_RGBTo1555(r, g, b) ((uint16_t)(((((r) & 255) >> 3) << 10) | ((((g) & 255) >> 3) << 5) | (((b) & 255) >> 3)))
void gfx_Begin(void);
void gfx_End(void);
void gfx_SetDrawBuffer(void);
void gfx_SwapDraw(void);
void gfx_SetColor(uint8_t c);
void gfx_FillRectangle(int x, int y, int w, int h);
void gfx_Rectangle(int x, int y, int w, int h);
void gfx_HorizLine(int x, int y, int len);
void gfx_VertLine(int x, int y, int len);
void gfx_Line(int x0, int y0, int x1, int y1);
void gfx_FillCircle(int x, int y, int r);
void gfx_FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2);
void gfx_SetTextFGColor(uint8_t c);
void gfx_SetTextBGColor(uint8_t c);
void gfx_SetTextTransparentColor(uint8_t c);
void gfx_SetTextScale(uint8_t w, uint8_t h);
typedef struct { uint8_t width, height; uint8_t data[]; } gfx_sprite_t;
void gfx_Sprite(const gfx_sprite_t *sprite, int x, int y);
void gfx_ScaledSprite_NoClip(const gfx_sprite_t *sprite, int x, int y, uint8_t w, uint8_t h);
void gfx_PrintStringXY(const char *s, int x, int y);
#endif
