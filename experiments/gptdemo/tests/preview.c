/* Runs GPTDEMO's real screen code against the stand-in graphics library and saves pictures (PPM files).
 * Usage: preview <scene> <outprefix>   scenes: splash, chat, stream, gallery, analyze */
#include <stdio.h>
#include <stdlib.h>
#define main app_main
#include "../src/main.c"
#undef main

extern uint8_t fb[240][320];
extern char text_log[];
extern uint32_t fake_ticks;
extern void (*swap_hook)(void);
extern void (*scan_hook)(void);

static const char *prefix;
static int frame, scan_no, shots[8], n_shots, shot_no;
static int key_frame[8], key_group[8], key_bit[8], n_keys;

static void save(void)
{
    char name[200];
    FILE *f;
    int x, y;
    sprintf(name, "%s_%d.ppm", prefix, shot_no++);
    f = fopen(name, "wb");
    fprintf(f, "P6\n320 240\n255\n");
    for (y = 0; y < 240; y++) for (x = 0; x < 320; x++) {
        uint16_t p = gfx_palette[fb[y][x]];
        fputc(((p >> 10) & 31) << 3, f); fputc(((p >> 5) & 31) << 3, f); fputc((p & 31) << 3, f);
    }
    fclose(f);
    sprintf(name, "%s_%d.txt", prefix, shot_no - 1);
    f = fopen(name, "w");
    fputs(text_log, f);
    fclose(f);
}

static void on_swap(void)
{
    int i;
    frame++;
    fake_ticks += TICKS_PER_SECOND / 20;   /* 50 ms per picture */
    for (i = 0; i < n_shots; i++) if (shots[i] == frame) save();
}

static void on_scan(void)
{
    int i;
    scan_no++;
    for (i = 0; i < 8; i++) kb_Data[i] = 0;
    for (i = 0; i < n_keys; i++) if (key_frame[i] == scan_no) kb_Data[key_group[i]] |= (uint8_t)(1 << key_bit[i]);   /* the key is down for one keyboard check */
}

static void shot_at(int f) { shots[n_shots++] = f; }
static void key_at(int f, int g, int b) { key_frame[n_keys] = f; key_group[n_keys] = g; key_bit[n_keys] = b; n_keys++; }

int main(int argc, char **argv)
{
    const char *scene = argc > 1 ? argv[1] : "chat";
    prefix = argc > 2 ? argv[2] : "/tmp/gptdemo";
    swap_hook = on_swap;
    scan_hook = on_scan;
    set_palette();
    if (!strcmp(scene, "splash")) {
        shot_at(6); shot_at(26); shot_at(46);
        splash();
    } else if (!strcmp(scene, "analyze")) {
        shot_at(14); shot_at(42); shot_at(54);
        analyze(1);
    } else if (!strcmp(scene, "gallery")) {
        shot_at(2); shot_at(6);
        key_at(6, 7, 2);   /* Right, at the 6th keyboard check */
        key_at(20, 6, 0);  /* Enter, at the 20th */
        gallery();
    } else {
        add_message(ROLE_USER, "what is sin 30 degrees", 0);
        add_message(ROLE_AI, script_reply("sin 30"), 0);
        msgs[n_msgs - 1].shown = (int)strlen(msgs[n_msgs - 1].text);
        add_message(ROLE_PHOTO, "", 0);
        assistant_says(PHOTOS[0].answer, 0, 70);
        if (!strcmp(scene, "chat")) {
            while (streaming >= 0) { fake_ticks += TICKS_PER_SECOND / 20; advance_stream(); }
        } else {
            while (streaming >= 0 && msgs[streaming].shown < 70) { fake_ticks += TICKS_PER_SECOND / 20; advance_stream(); }
        }
        strcpy(input, "explain the law of sines");
        input_len = (int)strlen(input);
        shot_at(1);
        draw_chat();
    }
    return 0;
}
