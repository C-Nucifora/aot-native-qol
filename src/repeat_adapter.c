#include "repeat_adapter.h"

/* Native table RVA 8955c0; mappings are preserved, including face buttons.
 * These masks identify only the native repeat output, never raw input axes. */
static const uint32_t masks[16] = {
    0x400, 0x800, 0x200000, 0x800000, 0x1000000, 0x400000,
    2, 4, 1, 8, 0x40, 0x80, 0x10, 0x20, 0x100, 0x200
};
static uint32_t read32(const unsigned char *p) {
    return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void write32(unsigned char *p, uint32_t v) {
    unsigned i; for (i=0; i<4; ++i) p[i]=(unsigned char)(v>>(8*i));
}
static int ticks(RepeatAdapter *a, double *out) {
    double seconds;
    if (!a->clock || !a->clock(a->clock_context, &seconds) ||
        !(seconds > 0 && seconds <= .05000001)) return 0;
    *out = seconds * 60.;
    return 1;
}
void repeat_reset(RepeatAdapter *a) {
    unsigned i;
    for (i=0; i<2; ++i) { a->lanes[i].state=0; a->lanes[i].fraction=0; }
}
void repeat_keyboard(RepeatAdapter *a, void *keyboard, float stock_ticks) {
    double step;
    a->keyboard(keyboard, ticks(a, &step) ? (float)step : stock_ticks);
}
int repeat_controller(RepeatAdapter *a, void *state, void *device,
                      unsigned stock_ticks) {
    unsigned char *s = state;
    unsigned char before[16];
    double step;
    unsigned lane, advance, i;
    uint32_t held, pressed, repeat;
    int result;
    if (!ticks(a, &step) || stock_ticks != 1) {
        repeat_reset(a);
        return a->controller(state, device, stock_ticks);
    }
    for (lane = 0; lane < 2; ++lane)
        if (a->lanes[lane].state == state) break;
    if (lane == 2) {
        for (lane = 0; lane < 2; ++lane)
            if (!a->lanes[lane].state) break;
        /* Unknown third input object: leave it native; no eviction mid-hold. */
        if (lane == 2) return a->controller(state, device, stock_ticks);
        a->lanes[lane].state = state;
    }
    a->lanes[lane].fraction += step;
    /* Boundary tolerance avoids a lost 60 Hz tick after repeated division. */
    advance = (unsigned)(a->lanes[lane].fraction + 1e-9);
    a->lanes[lane].fraction -= advance;
    if (a->lanes[lane].fraction < 0) a->lanes[lane].fraction = 0;
    for (i=0; i<16; ++i) before[i]=s[0x34+i];
    /* Original still polls every frame and generates edges/deadzones/axes.
     * Only repeat-counter advancement is supplied separately below. */
    result = a->controller(state, device, 0);
    held = read32(s + 4);
    pressed = read32(s + 8);
    repeat = read32(s + 12);
    for (i = 0; i < 16; ++i) {
        unsigned counter = before[i], j;
        int pulse = 0;
        if (counter > 19) {
            /* Foreign/unexpected state: preserve native result for this key. */
            continue;
        }
        if (!(held & masks[i])) counter = 0;
        else for (j = 0; j < advance; ++j) {
            if (++counter >= 20) { counter = 16; pulse = 1; }
            else if (counter == 16) pulse = 1;
        }
        s[0x34 + i] = (unsigned char)counter;
        repeat &= ~masks[i];
        if ((pressed & masks[i]) || pulse) repeat |= masks[i];
    }
    write32(s + 12, repeat);
    return result;
}
