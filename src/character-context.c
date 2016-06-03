/* character-context.c - pokgame */
#include "character-context.h"
#include "error.h"
#include "pokgame.h"
#include "primatives.h"
#include <stdlib.h>

/* pok_character_context */
static struct pok_character_context* pok_character_context_new(struct pok_character* character)
{
    struct pok_character_context* context;
    context = malloc(sizeof(struct pok_character_context));
    if (context == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    context->character = character;
    /* get frame based on initial direction */
    context->frame = pok_to_frame_direction(character->direction);
    context->offset[0] = 0;
    context->offset[1] = 0;
    context->shadow = FALSE;
    context->eff = pok_character_no_effect;
    context->resolveFrame = 0;
    context->spinRate = 0;
    context->spinTicks = 0;
    context->granularity = 4;
    context->slowDown = FALSE;
    context->aniTicks = 0;
    context->aniTicksAmt = 30;
    context->frameAlt = 0;
    context->update = FALSE;
    return context;
}
static void pok_character_context_render(struct pok_character_context* context,const struct pok_map_render_context* mapRC,
    const struct pok_sprite_manager* sman,const struct pok_graphics_subsystem* sys)
{
    /* check to see if the character is within the viewing area defined by the
       map render context; if the render context has no loaded maps, then do
       nothing */
    struct pok_character* ch = context->character;
    if (mapRC->map != NULL && ch->mapNo == mapRC->map->mapNo) { /* same map */
        int i;
        for (i = 0;i < 4;++i) {
            if (mapRC->info[i].chunk != NULL && mapRC->info[i].chunkPos.X == ch->chunkPos.X
                      && mapRC->info[i].chunkPos.Y == ch->chunkPos.Y) { /* same chunk */
                int32_t cols, rows;
                cols = ch->tilePos.column - mapRC->info[i].loc.column;
                if (cols >= 0 && cols < mapRC->info[i].across) { /* same viewable column space */
                    rows = ch->tilePos.row - mapRC->info[i].loc.row;
                    if (rows >= 0 && rows < mapRC->info[i].down) { /* same viewable row space */
                        int32_t x, y;
                        x = mapRC->info[i].px + cols * sys->dimension + mapRC->offset[0] + sys->playerOffsetX;
                        y = mapRC->info[i].py + rows * sys->dimension + mapRC->offset[1] + sys->playerOffsetY;
                        pok_image_render(
                            sman->spriteassoc[ch->spriteIndex][context->frame],
                            x + context->offset[0],
                            y + context->offset[1] );
                        if (context->shadow) {
                            /* draw the shadow (the character is probably hopping over something) */
                            pok_primative_setup_modelview(
                                x + sys->dimension / 2,
                                y + sys->dimension + context->offset[1] / 2,
                                sys->dimension,
                                sys->dimension );
                            glVertexPointer(2,GL_FLOAT,0,POK_SHADOW_ELLIPSE);
                            glColor4f(BLACK_PIXEL_FLOAT[0],BLACK_PIXEL_FLOAT[1],BLACK_PIXEL_FLOAT[2],0.5);
                            glDrawArrays(GL_POLYGON,0,POK_SHADOW_ELLIPSE_VERTEX_COUNT);
                            glLoadIdentity();
                        }
                        break;
                    }
                }
            }
        }
    }
}
bool_t pok_character_context_move(struct pok_character_context* context,enum pok_direction direction)
{
    /* this function updates a character in the specified direction; this is the basic logic that the
       engine performs on NPC's; the character is never updated: just an update is performed for
       animation's sake */

    return FALSE;
}
void pok_character_context_set_player(struct pok_character_context* context,struct pok_map_render_context* mapRC)
{
    /* the player character is aligned with the map render context */
    context->character->mapNo = mapRC->map->mapNo;
    context->character->chunkPos = mapRC->chunkpos;
    context->character->tilePos = mapRC->relpos;
}
void pok_character_context_set_update(struct pok_character_context* context,
    enum pok_direction direction,
    enum pok_character_effect effect,
    uint16_t parameter,
    bool_t resetTime)
{
    context->eff = effect;
    if (effect == pok_character_normal_effect || effect == pok_character_slide_effect) {
        /* set the resolve frame and animation frame; recall that up/down directions have two frames instead of one;
           the frame alternation counter is used to alternate between those frames */
        context->character->direction = direction;
        context->resolveFrame = pok_to_frame_direction(direction);
        /* set sprite offset (only if 'dimension' was non-zero) */
        if (parameter) {
            switch (direction) {
            case pok_direction_up:
                context->offset[1] = parameter;
                break;
            case pok_direction_down:
                context->offset[1] = -parameter;
                break;
            case pok_direction_left:
                context->offset[0] = parameter;
                break;
            case pok_direction_right:
                context->offset[0] = -parameter;
                break;
            default:
                break;
            }
        }
        /* 'context->update' will act as a counter that determines when
           the animation frames change; it is set to +1 the granularity
           so that we can wait one cycle before applying the animation
           frame */
        context->update = context->granularity + 1;
    }
    else if (effect == pok_character_jump_effect) {
        context->character->direction = direction;
        context->resolveFrame = pok_to_frame_direction(direction);
        context->shadow = TRUE;
        /* set sprite offset to double the dimension (jump skips a tile); the jump only
           occurs in the down, left and right directions */
        parameter <<= 1;
        switch (direction) {
        case pok_direction_down:
            context->offset[1] = -parameter;
            break;
        case pok_direction_left:
            context->offset[0] = parameter;
            break;
        case pok_direction_right:
            context->offset[0] = -parameter;
            break;
        default:
            break;
        }
        /* let 'context->update' hold the number of iterations for the update; let a jump
           take twice as long as a normal update */
        context->update = context->granularity << 1;
    }
    else if (effect == pok_character_spin_effect) {
        /* the spin effect causes the player to spin in place; the parameter determines
           how many update cycles occur before the player is spun next; we must take (at
           least) as much time as is required to spin the player around once */
        int t = pok_direction_cycle_distance(context->character->direction,direction,1,FALSE);
        context->resolveFrame = pok_to_frame_direction(direction);

        if (parameter > 0) {
            /* remember spin rate in this variable so we can advance the
               animation frames at the specified interval */
            context->spinRate = parameter;
        }
        else {
            if (context->spinRate == 0)
                pok_error(pok_error_fatal,"character context spin rate was zero in pok_character_context_set_update()",1);
            /* remember spin rate from previous iteration */
            parameter = context->spinRate;
        }

        /* 'context->update' will hold the number of iterations for the update */
        context->update = t;
        context->spinTicks = 0;
        if (context->update == 0)
            context->update = 1;
    }

    /* reset tick counter if specified; otherwise keep any extra time
       from the previous timeout (this helps keep game time consistent) */
    if (resetTime)
        context->aniTicks = 0;
}
static bool_t pok_character_context_normal_update(struct pok_character_context* context,int inc)
{
    /* update character frame if we have waited one cycle */
    if (context->update == context->granularity)
        /* increment the frame alteration counter so that animation frames alternate */
        context->frame = context->resolveFrame + 1/*ani*/ + context->frameAlt++ % 2 /*alt-ani*/;
    /* update animation offset */
    if (context->offset[0] < 0) {
        context->offset[0] += inc;
        if (context->offset[0] > 0)
            context->offset[0] = 0;
    }
    else if (context->offset[0] > 0) {
        context->offset[0] -= inc;
        if (context->offset[0] < 0)
            context->offset[0] = 0;
    }
    else if (context->offset[1] < 0) {
        context->offset[1] += inc;
        if (context->offset[1] > 0)
            context->offset[1] = 0;
    }
    else if (context->offset[1] > 0) {
        context->offset[1] -= inc;
        if (context->offset[1] < 0)
            context->offset[1] = 0;
    }
    /* decrement update counter and see if it is time to resolve to
       our destination frame; check slightly before end of rendering
       sequence to allow more time to view the frame (in case the sequence
       starts back up immediately) */
    if (--context->update == context->granularity / 4)
        context->frame = context->resolveFrame;
    /* check to see if we are finished; opt out of the last frame
       (it's the same as the start of the next sequence) */
    if (context->update == 1 && context->offset[0] == 0 && context->offset[1] == 0) {
        /* done: we skip the last frame because it is redundant; return TRUE to mean
           that the process completed */
        context->update = FALSE;
        return TRUE;
    }
    return FALSE;
}
static bool_t pok_character_context_slide_update(struct pok_character_context* context,int inc)
{
    /* update character frame after waiting one cycle; since the character is sliding, no animation frames are used */
    if (context->update == context->granularity)
        context->frame = context->resolveFrame;
    /* update animation offset */
    if (context->offset[0] < 0) {
        context->offset[0] += inc;
        if (context->offset[0] > 0)
            context->offset[0] = 0;
    }
    else if (context->offset[0] > 0) {
        context->offset[0] -= inc;
        if (context->offset[0] < 0)
            context->offset[0] = 0;
    }
    else if (context->offset[1] < 0) {
        context->offset[1] += inc;
        if (context->offset[1] > 0)
            context->offset[1] = 0;
    }
    else if (context->offset[1] > 0) {
        context->offset[1] -= inc;
        if (context->offset[1] < 0)
            context->offset[1] = 0;
    }
    /* check to see if finished */
    if (--context->update == 1 && context->offset[0] == 0 && context->offset[1] == 0) {
        /* done: we skip the last frame because it is redundant; return TRUE to mean
           that the process completed */
        context->update = FALSE;
        return TRUE;
    }    
    return FALSE;
}
static bool_t pok_character_context_jump_update(struct pok_character_context* context,int inc)
{
    /* update character frame if we have waited one cycle */
    if (context->update == (context->granularity << 1) - 1)
        /* increment the frame alteration counter so that animation frames alternate */
        context->frame = context->resolveFrame + 1/*ani*/ + context->frameAlt++ % 2 /*alt-ani*/;
    /* update vertical offset */
    context->offset[1] += (context->update > context->granularity ? -inc : inc);
    /* update animation offset */
    if (context->offset[0] < 0) {
        context->offset[0] += inc;
        if (context->offset[0] > 0)
            context->offset[0] = 0;
    }
    else if (context->offset[0] > 0) {
        context->offset[0] -= inc;
        if (context->offset[0] < 0)
            context->offset[0] = 0;
    }
    else if (context->offset[1] < 0) {
        context->offset[1] += inc;
        if (context->offset[1] > 0)
            context->offset[1] = 0;
    }
    if (!--context->update && context->offset[0] == 0 && context->offset[1] == 0) {
        context->frame = context->resolveFrame;
        context->shadow = FALSE;
        return TRUE;
    }
    return FALSE;
}
static bool_t pok_character_context_spin_update(struct pok_character_context* context,int gameTicks)
{
    if (context->update > 0) {
        /* determine number of spin cycles that have elapsed */
        uint16_t t = (context->spinTicks += gameTicks) / context->spinRate;
        context->spinTicks %= context->spinRate;

        if (t > 0) {
            /* for good configuration, t should be close to 1; we apply the correct number to imitate (as best we
               can) the desired spin rate */
            for (uint16_t i = 0;i < t && context->update;++i,--context->update) {
                /* change the animation frame by one counter-clockwise rotation */
                context->character->direction = pok_direction_counterclockwise_next(context->character->direction);
                context->frame = pok_to_frame_direction(context->character->direction);
            }
        }

        if (context->update)
            return FALSE;
    }
    context->character->direction = pok_from_frame_direction(context->resolveFrame);
    context->frame = context->resolveFrame;

    return TRUE;
}
bool_t pok_character_context_update(struct pok_character_context* context,uint16_t dimension,uint32_t ticks)
{
    if (context->update) {
        /* spin updates don't need a computed increment amount and require
           directly the elapsed game time */
        if (context->eff == pok_character_spin_effect)
            return pok_character_context_spin_update(context,ticks);

        /* make sure enough elapsed time has occurred before performing the update operation */
        int inc;
        int times;
        uint32_t amt = context->slowDown ? 2 * context->aniTicksAmt : context->aniTicksAmt;
        context->aniTicks += ticks;
        if (context->aniTicks >= amt) {
            /* compute increment amount and number of times to apply it */
            inc = dimension / context->granularity;
            times = context->aniTicks / amt;
            if (inc == 0)
                /* granularity was too fine */
                inc = times;
            else
                inc *= times;
            /* remember any leftover ticks so that we can keep time */
            context->aniTicks %= amt;
            /* update character context based on which effect is being applied */
            if (context->eff == pok_character_normal_effect)
                return pok_character_context_normal_update(context,inc);
            else if (context->eff == pok_character_slide_effect)
                return pok_character_context_slide_update(context,inc);
            else if (context->eff == pok_character_jump_effect)
                return pok_character_context_jump_update(context,inc);
        }
    }
    return FALSE;
}

/* pok_character_render_context */
struct pok_character_render_context* pok_character_render_context_new(const struct pok_map_render_context* mapRC,
    const struct pok_sprite_manager* sman)
{
    struct pok_character_render_context* context;
    context = malloc(sizeof(struct pok_character_render_context));
    if (context == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_character_render_context_init(context,mapRC,sman);
    return context;
}
void pok_character_render_context_free(struct pok_character_render_context* context)
{
    pok_character_render_context_delete(context);
    free(context);
}
void pok_character_render_context_init(struct pok_character_render_context* context,const struct pok_map_render_context* mapRC,
    const struct pok_sprite_manager* sman)
{
    dynamic_array_init(&context->chars);
    context->mapRC = mapRC;
    context->sman = sman;
}
void pok_character_render_context_delete(struct pok_character_render_context* context)
{
    dynamic_array_delete_ex(&context->chars,free);
}
bool_t pok_character_render_context_add(struct pok_character_render_context* context,struct pok_character* character)
{
    size_t i;
    struct pok_character_context* cc = pok_character_context_new(character);
    if (cc == NULL)
        return FALSE;
    for (i = 0;i < context->chars.da_top;++i) {
        if (context->chars.da_data[i] == NULL) {
            context->chars.da_data[i] = cc;
            return TRUE;
        }
    }
    dynamic_array_pushback(&context->chars,cc);
    return TRUE;
}
struct pok_character_context* pok_character_render_context_add_ex(struct pok_character_render_context* context,
    struct pok_character* character)
{
    size_t i;
    struct pok_character_context* cc = pok_character_context_new(character);
    if (cc == NULL)
        return NULL;
    for (i = 0;i < context->chars.da_top;++i) {
        if (context->chars.da_data[i] == NULL) {
            context->chars.da_data[i] = cc;
            return cc;
        }
    }
    dynamic_array_pushback(&context->chars,cc);
    return cc;
}
bool_t pok_character_render_context_remove(struct pok_character_render_context* context,struct pok_character* character)
{
    size_t i;
    struct pok_character_context* cc;
    for (i = 0;i < context->chars.da_top;++i) {
        cc = context->chars.da_data[i];
        if (cc->character == character) {
            context->chars.da_data[i] = NULL;
            free(cc);
            return TRUE;
        }
    }
    return FALSE;
}

/* rendering routine */
void pok_character_render(const struct pok_graphics_subsystem* sys,struct pok_character_render_context* context)
{
    /* go through each character context and render it; we must lock for read access so that we don't
       access the dynamic array member while in an invalidated state */
    size_t i;
    pok_game_lock(context);
    for (i = 0;i < context->chars.da_top;++i)
        pok_character_context_render(
            context->chars.da_data[i],
            context->mapRC,
            context->sman,
            sys );
    pok_game_unlock(context);
}
