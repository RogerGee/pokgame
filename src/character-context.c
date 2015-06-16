/* character-context.c - pokgame */
#include "character-context.h"
#include "error.h"
#include "pokgame.h"
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
    /* set map and chunk references to NULL initially; they will be used by the
       implementation; if they remain NULL, then this means that the character
       should not be managed by the update functionality (such as with the user's character) */
    context->map = NULL;
    context->chunk = NULL;
    /* get frame based on initial direction */
    context->frame = pok_to_frame_direction(character->direction);
    context->offset[0] = 0;
    context->offset[1] = 0;
    context->shadow = FALSE;
    context->eff = pok_character_no_effect;
    context->resolveFrame = 0;
    context->granularity = 1;
    context->slowDown = FALSE;
    context->aniTicks = 0;
    context->aniTicksAmt = 1;
    context->frameAlt = 0;
    context->update = FALSE;
    return context;
}
static void pok_character_context_render(struct pok_character_context* context,const struct pok_map_render_context* mapRC,
    const struct pok_sprite_manager* sman,const struct pok_graphics_subsystem* sys)
{
    /* check to see if the character is within the viewing area defined by the map render context */
    if (context->character->mapNo == mapRC->map->mapNo) { /* same map */
        int i;
        for (i = 0;i < 4;++i) {
            if (mapRC->info[i].chunk != NULL && mapRC->info[i].chunkPos.X == context->character->chunkPos.X
                      && mapRC->info[i].chunkPos.Y == context->character->chunkPos.Y) { /* same chunk */
                int cols, rows;
                cols = context->character->tilePos.column - mapRC->info[i].loc.column;
                if (cols >= 0 && cols < mapRC->info[i].across) { /* same viewable column space */
                    rows = context->character->tilePos.row - mapRC->info[i].loc.row;
                    if (rows >= 0 && rows < mapRC->info[i].down) { /* same viewable row space */
                        pok_image_render(
                            sman->spriteassoc[context->character->spriteIndex][context->frame],
                            mapRC->info[i].px + cols * sys->dimension + sys->playerOffsetX + context->offset[0] + mapRC->offset[0],
                            mapRC->info[i].py + rows * sys->dimension + sys->playerOffsetY + context->offset[1] + mapRC->offset[1]);
                        break;
                    }
                }
            }
        }
    }
}
bool_t pok_character_context_move(struct pok_character_context* context,enum pok_direction direction)
{
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
    uint16_t parameter)
{
    context->eff = effect;
    if (effect == pok_character_normal_effect) {
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

    /* reset tick counter */
    context->aniTicks = 0;
}
bool_t pok_character_context_update(struct pok_character_context* context,uint16_t dimension,uint32_t ticks)
{
    if (context->update) {
        /* update character context based on which effect is being applied; make sure enough elapsed time
           has occurred before performing the update operation */
        if (context->eff == pok_character_normal_effect) {
            uint32_t amt = context->slowDown ? 2 * context->aniTicksAmt : context->aniTicksAmt;
            context->aniTicks += ticks;
            if (context->aniTicks >= amt) {
                int inc;
                int times;
                /* compute increment amount and number of times to apply it */
                inc = dimension / context->granularity;
                times = context->aniTicks / amt;
                if (inc == 0)
                    /* granularity was too fine */
                    inc = times;
                else
                    inc *= times;
                /* update character frame if at the right point in the sequence; we
                   have waited one cycle if so */
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
                /* check to see if we are finished; opt out of the last frame
                   (it's the same as the start of the next sequence) */
                if (--context->update == 1 && context->offset[0] == 0 && context->offset[1] == 0) {
                    /* done: context->update is now false; return TRUE to mean
                       that the process completed */
                    context->frame = context->resolveFrame;
                    return TRUE;
                }
                /* remember any leftover ticks so that we can keep time */
                context->aniTicks %= amt;
            }
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
