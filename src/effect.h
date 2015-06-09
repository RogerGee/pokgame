/* effect.h - pokgame */
#ifndef POKGAME_EFFECT_H
#define POKGAME_EFFECT_H
#include "graphics.h"

/* pok_effect: base class for effects */
struct pok_effect
{
    bool_t update;     /* is the effect being updated? */
    uint32_t ticksAmt; /* ticks required each update interval (each stage of the update) */
    uint32_t ticks;    /* accumulated ticks up to this point */
};

enum pok_fadeout_effect_kind
{
    pok_fadeout_black_screen,  /* dim the entire screen to black */
    pok_fadeout_to_center      /* paint black around the screen until reaching the center */
};

/* pok_fadeout_effect: an effect that produces an animated fadeout */
struct pok_fadeout_effect
{
    struct pok_effect _base;     /* base */
    bool_t reverse;              /* if non-zero, then reverse the fade out */
    bool_t keep;                 /* if non-zero, then keep screen with fade out */
    uint32_t delay;              /* initial delay before applying fade in (complements 'keep') */
    uint8_t kind;                /* kind of effect to use */

    /* contextual information for implementation */
    float alpha;
    float hs[4];
    float d[2];
};
void pok_fadeout_effect_init(struct pok_fadeout_effect* effect);
void pok_fadeout_effect_set_update(struct pok_fadeout_effect* effect,
    const struct pok_graphics_subsystem* sys,
    uint32_t time,
    uint8_t kind,
    bool_t reverse);
void pok_fadeout_effect_update(struct pok_fadeout_effect* effect,uint32_t ticks);
void pok_fadeout_effect_render(struct pok_graphics_subsystem* sys,const struct pok_fadeout_effect* effect);

#endif
