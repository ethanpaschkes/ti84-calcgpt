#include "payload.h"
#include <string.h>

bool file_next_var(const file_t *f, size_t *pos, var_t *v)
{
    size_t len = f->end - f->start, end, hdr;
    if (len < 0x37 || memcmp(f->start, "**TI83F*", 8) != 0) return false;
    end = 0x37 + (f->start[0x35] | (f->start[0x36] << 8));
    if (end > len) return false;
    if (*pos < 0x37) *pos = 0x37;
    if (*pos + 17 > end) return false;
    hdr = f->start[*pos] | (f->start[*pos + 1] << 8);
    v->size = f->start[*pos + 2] | (f->start[*pos + 3] << 8);
    v->type = f->start[*pos + 4];
    memcpy(v->name, f->start + *pos + 5, 8);
    v->name[8] = 0;
    v->version = hdr >= 0x0D ? f->start[*pos + 13] : 0;
    v->archived = hdr >= 0x0D && (f->start[*pos + 14] & 0x80);
    v->data = f->start + *pos + 2 + hdr + 2;
    if (v->data + v->size > f->start + end) return false;
    *pos = v->data + v->size - f->start;
    return true;
}

bool file_flash_app(const file_t *f, var_t *v)
{
    size_t len = f->end - f->start;
    uint32_t size;
    if (len < 0x4E || memcmp(f->start, "**TIFL**", 8) != 0 || f->start[0x31] != TYPE_APP) return false;
    size = f->start[0x4A] | ((uint32_t)f->start[0x4B] << 8) | ((uint32_t)f->start[0x4C] << 16) | ((uint32_t)f->start[0x4D] << 24);
    if (size == 0 || 0x4E + size > len) return false;
    memcpy(v->name, f->start + 0x12, 8);
    v->name[8] = 0;
    v->type = TYPE_APP;
    v->version = 0;
    v->archived = true;
    v->data = f->start + 0x4E;
    v->size = size;
    return true;
}

int run_plan(const uint8_t os[3])
{
    if (os[0] < 5 || (os[0] == 5 && os[1] < 5)) return RUN_NATIVE;
    if (os[0] == 5 && os[1] < 8) return RUN_ARTIFICE2;
    if (os[0] == 5 && os[1] == 8 && os[2] < 5) return RUN_ARTIFICE2;
    if (os[0] == 5 && os[1] == 8 && os[2] == 5) return RUN_ARTIFICE3;
    return RUN_UNSUPPORTED;
}

const char *run_plan_text(int plan)
{
    switch (plan) {
    case RUN_NATIVE: return "this OS runs C programs as they are: press prgm, run CALC";
    case RUN_ARTIFICE2: return "OS 5.5-5.8.4: press prgm, run A (arTIfiCE), pick CALC in its list (every time the calculator restarts)";
    case RUN_ARTIFICE3: return "OS 5.8.5: AsmHook2 is set up once through Inequalz; after that press prgm, run CALC";
    default: return "this OS is newer than any jailbreak this board knows: C programs cannot run on it yet";
    }
}
