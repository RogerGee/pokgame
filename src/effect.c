/* effect.c - pokgame */
#include "effect.h"
#include "error.h"
#include "opengl.h"
#include "primatives.h"

/* constant parameters for effects; time is in milliseconds */
#define MAX_ALPHA                    1.0
#define MIN_ALPHA                   -1.0
#define FADEOUT_EFFECT_GRANULARITY   100
#define DAYCYCLE_CLOCK_CHECK       60000
static const float NIGHT_PIXEL_FLOAT[] = {0.0f,0.0f,0.4570f};
#define NIGHT_ALPHA                 0.5f
static const float MORNING_PIXEL_FLOAT[] = {0.75f,1.0f,0.0f};
#define MORNING_ALPHA               0.25f

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
    if (effect->_base.ticksAmt == 0)
        /* we'll have to approximate */
        effect->_base.ticksAmt = 1;
    if (kind == pok_fadeout_black_screen) {
        /* if we go in reverse, start with fully opaque black background, otherwise
           start fully transparent */
        effect->alpha = (float) (reverse ? MAX_ALPHA : MIN_ALPHA);
    }
    else if (kind == pok_fadeout_to_center) {
        /* heights of quads: top, bottom, left, right */
        if (reverse) {
            effect->hs[0] = effect->hs[1] = sys->wheight / (float)2.0;
            effect->hs[2] = effect->hs[3] = sys->wwidth / (float)2.0;
        }
        else {
            effect->hs[0] = 0.0;
            effect->hs[1] = (float)sys->wheight;
            effect->hs[2] = 0.0;
            effect->hs[3] = (float)sys->wwidth;
        }
        /* distance deltas */
        effect->d[0] = (float) (sys->wwidth / 2.0 / FADEOUT_EFFECT_GRANULARITY); /* left and right */
        effect->d[1] = (float) (sys->wheight / 2.0 / FADEOUT_EFFECT_GRANULARITY); /* top and bottom */
        if (reverse) {
            effect->d[0] *= -1;
            effect->d[1] *= -1;
        }
    }
#ifdef POKGAME_DEBUG
    else
        pok_error(pok_error_fatal,"bad parameter 'kind' to pok_fadeout_effect_set_update()");
#endif
    effect->_base.update = TRUE;
}
bool_t pok_fadeout_effect_update(struct pok_fadeout_effect* effect,uint32_t ticks)
{
    if (effect->_base.update) {
        uint32_t times = pok_effect_elapsed(&effect->_base,ticks);
        if (effect->reverse) {
            /* apply the delay if we are going in the reverse direction; this
               will add time to the sequence */
            effect->delay = ticks > effect->delay ? 0 : (effect->delay - ticks);
            if (effect->delay > 0)
                return FALSE;
        }
        if (times == 0)
            return FALSE;
        switch (effect->kind) {
        case pok_fadeout_black_screen: {
                float d = effect->alpha;
                if (effect->reverse) {
                    /* alpha component --> 0 */
                    d -= ((float)2.0 / FADEOUT_EFFECT_GRANULARITY) * times;
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
                    d += ((float)2.0 / FADEOUT_EFFECT_GRANULARITY) * times;
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
                /* apply distance deltas to current bounding quad heights */
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
        if ( !effect->_base.update ) {
            effect->keep = !effect->reverse && effect->keep;
            return TRUE;
        }
    }
    return FALSE;
}
void pok_fadeout_effect_render(struct pok_graphics_subsystem* sys,const struct pok_fadeout_effect* effect)
{
    if (effect->_base.update) {
        if (effect->kind == pok_fadeout_black_screen) {
            /* set color to black; include alpha */
            pok_primative_setup_modelview(sys->wwidth/2,sys->wheight/2,sys->wwidth,sys->wheight);
            glVertexPointer(2,GL_FLOAT,0,POK_BOX);
            glColor4f(BLACK_PIXEL_FLOAT[0],BLACK_PIXEL_FLOAT[1],BLACK_PIXEL_FLOAT[2],effect->alpha);
            glDrawArrays(GL_POLYGON,0,POK_BOX_VERTEX_COUNT);
            glLoadIdentity();
        }
        else if (effect->kind == pok_fadeout_to_center) {
            /* draw 4 quadrilaterals */
            glColor3b(BLACK_PIXEL.r,BLACK_PIXEL.g,BLACK_PIXEL.b);
            glBegin(GL_QUAD_STRIP);
            { /* 10 verteces define 4 quadrilaterals */
                glVertex2f(0.0,0.0);
                glVertex2f(effect->hs[2],effect->hs[0]);
                glVertex2f((GLfloat)sys->wwidth,0.0);
                glVertex2f(effect->hs[3],effect->hs[0]);
                glVertex2f((GLfloat)sys->wwidth,(GLfloat)sys->wheight);
                glVertex2f(effect->hs[3],effect->hs[1]);
                glVertex2f(0.0,(GLfloat)sys->wheight);
                glVertex2f(effect->hs[2],effect->hs[1]);
                glVertex2f(0.0,0.0);
                glVertex2f(effect->hs[2],effect->hs[0]);
            }
            glEnd();
        }
    }
    else if (effect->keep) {
        /* keep the screen faded out */
        pok_primative_setup_modelview(sys->wwidth/2,sys->wheight/2,sys->wwidth,sys->wheight);
        glVertexPointer(2,GL_FLOAT,0,POK_BOX);
        glColor3b(BLACK_PIXEL.r,BLACK_PIXEL.g,BLACK_PIXEL.b);
        glDrawArrays(GL_POLYGON,0,POK_BOX_VERTEX_COUNT);
        glLoadIdentity();
    }
}

/* pok_daycycle_effect */
void pok_daycycle_effect_init(struct pok_daycycle_effect* effect)
{
    /* note: we will not use base::update nor base::ticksAmt */
    pok_effect_init(&effect->_base);
    effect->kind = pok_daycycle_time_clock;
    effect->fromClock = TRUE;
}
void pok_daycycle_effect_set_update(struct pok_daycycle_effect* effect,enum pok_daycycle_flag flag)
{
    effect->kind = flag;
    effect->fromClock = (pok_daycycle_time_clock == flag);
}
void pok_daycycle_effect_update(struct pok_daycycle_effect* effect,uint32_t ticks)
{
    if (effect->fromClock) {
        bool_t t;
        effect->_base.ticks += ticks;
        t = effect->_base.ticks >= DAYCYCLE_CLOCK_CHECK;
        effect->_base.ticks %= DAYCYCLE_CLOCK_CHECK;
        if (effect->kind == pok_daycycle_time_clock || t) {
            struct pok_datetime datetime;
            pok_datetime_init(&datetime);
            if (datetime.hour >= 4 && datetime.hour < 10)
                effect->kind = pok_daycycle_time_morning;
            else if (datetime.hour >= 10 && datetime.hour < 18)
                effect->kind = pok_daycycle_time_day;
            else
                effect->kind = pok_daycycle_time_night;
        }
    }

    /* otherwise there is nothing to update */
}
void pok_daycycle_effect_render(struct pok_graphics_subsystem* sys,const struct pok_daycycle_effect* effect)
{
    /* this function only applies an effect if it is night or morning */
    if (effect->kind != pok_daycycle_time_day && effect->kind != pok_daycycle_time_clock) {
        pok_primative_setup_modelview(sys->wwidth/2,sys->wheight/2,sys->wwidth,sys->wheight);
        glVertexPointer(2,GL_FLOAT,0,POK_BOX);
        if (effect->kind == pok_daycycle_time_night)
            glColor4f(NIGHT_PIXEL_FLOAT[0],NIGHT_PIXEL_FLOAT[1],NIGHT_PIXEL_FLOAT[2],NIGHT_ALPHA);
        else
            glColor4f(MORNING_PIXEL_FLOAT[0],MORNING_PIXEL_FLOAT[1],MORNING_PIXEL_FLOAT[2],MORNING_ALPHA);
        glDrawArrays(GL_POLYGON,0,POK_BOX_VERTEX_COUNT);
        glLoadIdentity();
    }
}
