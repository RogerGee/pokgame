/* effect.c - pokgame */
#include "effect.h"
#include "error.h"

/* include platform specific code for render functions */
#ifdef POKGAME_OPENGL
#include "effect-GL.c"
#endif

/* constant parameters for effects; time is in milliseconds */
#define MAX_ALPHA                    1.0
#define MIN_ALPHA                   -1.0
#define FADEOUT_EFFECT_GRANULARITY   100

/* pok_effect */
static void pok_effect_init(struct pok_effect* effect)
{
    effect->update = FALSE;
    effect->ticks = 0;
    effect->ticksAmt = 1;
}
static inline uint32_t pok_effect_elapsed(struct pok_effect* effect,uint32_t ticks)
{
    uint32_t d;
    effect->ticks += ticks;
    d = effect->ticks / effect->ticksAmt;
    effect->ticks %= effect->ticksAmt;
    return d;
}

/* pok_fadeout_effect */
void pok_fadeout_effect_init(struct pok_fadeout_effect* effect)
{
    int i;
    pok_effect_init(&effect->_base);
    effect->reverse = FALSE;
    effect->keep = FALSE;
    effect->delay = 0;
    effect->kind = pok_fadeout_black_screen;
    effect->alpha = MIN_ALPHA;
    for (i = 0;i < 4;++i)
        effect->hs[i] = 0.0;
    effect->d[0] = effect->d[1] = 0.0;
}
void pok_fadeout_effect_set_update(struct pok_fadeout_effect* effect,
    const struct pok_graphics_subsystem* sys,
    uint32_t time,
    uint8_t kind,
    bool_t reverse)
{
    effect->kind = kind;
    effect->reverse = reverse;
    effect->_base.ticksAmt = time / FADEOUT_EFFECT_GRANULARITY;
    if (kind == pok_fadeout_black_screen) {
        /* if we go in reverse, start with fully opaque black background, otherwise
           start fully transparent */
        effect->alpha = reverse ? MAX_ALPHA : MIN_ALPHA;
    }
    else if (kind == pok_fadeout_to_center) {
        /* heights of quads: top, bottom, left, right */
        if (reverse) {
            effect->hs[0] = effect->hs[1] = sys->wheight / 2.0;
            effect->hs[2] = effect->hs[3] = sys->wwidth / 2.0;
        }
        else {
            effect->hs[0] = 0.0;
            effect->hs[1] = sys->wheight;
            effect->hs[2] = 0.0;
            effect->hs[3] = sys->wwidth;
        }
        /* distance deltas */
        effect->d[0] = sys->wwidth / 2.0 / time; /* left and right */
        effect->d[1] = sys->wheight / 2.0 / time; /* top and bottom */
        if (reverse) {
            effect->d[0] *= -1;
            effect->d[0] *= -1;
        }
    }
#ifdef POKGAME_DEBUG
    else
        pok_error(pok_error_fatal,"bad parameter 'kind' to pok_fadeout_effect_set_update()");
#endif
    effect->_base.update = TRUE;
}
void pok_fadeout_effect_update(struct pok_fadeout_effect* effect,uint32_t ticks)
{
    if (effect->_base.update) {
        uint32_t i, times = pok_effect_elapsed(&effect->_base,ticks);
        if (effect->reverse) {
            /* apply the delay if we are going in the reverse direction; this
               will add time to the sequence */
            effect->delay = ticks > effect->delay ? 0 : effect->delay - ticks;
            if (effect->delay > 0)
                return;
        }
        if (times == 0)
            return;
        switch (effect->kind) {
        case pok_fadeout_black_screen: {
                float d = effect->alpha;
                if (effect->reverse) {
                    /* alpha component --> 0 */
                    d -= (2.0 / FADEOUT_EFFECT_GRANULARITY) * times;
                    /* assign and check for completion */
                    if (d <= MIN_ALPHA) {
                        effect->alpha = MIN_ALPHA;
                        effect->_base.update = FALSE;
                    }
                    else
                        effect->alpha = d;
                }
                else {
                    /* alpha component --> 0xff */
                    d += (2.0 / FADEOUT_EFFECT_GRANULARITY) * times;
                    /* assign and check for completion */
                    if (d >= MAX_ALPHA) {
                        effect->alpha = MAX_ALPHA;
                        effect->_base.update = FALSE;
                    }
                    else
                        effect->alpha = d;
                }
                break;
            }
        case pok_fadeout_to_center: { 
                float ds[] = {effect->d[0] * times, effect->d[1] * times};
                effect->hs[0] += ds[1];
                effect->hs[1] -= ds[1];
                effect->hs[2] += ds[0];
                effect->hs[3] -= ds[0];
                /* check to see if the sequence is finished */
                if (effect->reverse) {
                    if (effect->hs[0] <= 0.0 || effect->hs[3] <= 0.0)
                        effect->_base.update = FALSE;
                }
                else if (effect->hs[0] >= effect->hs[1] || effect->hs[2] >= effect->hs[3])
                    effect->_base.update = FALSE;
                break;
            }
        }
        /* keep the effect only if we were dimming and the operation
           is complete (the screen is completely black) and the user
           had marked keep */
        if ( !effect->_base.update )
            effect->keep = !effect->reverse && effect->keep;
    }
}
