/* character-context.h - pokgame */
#ifndef POKGAME_CHARACTER_CONTEXT_H
#define POKGAME_CHARACTER_CONTEXT_H
#include "map-context.h"
#include "spriteman.h"
#include "character.h"
#include <dstructs/dynarray.h>

/* Notes about character rendering: characters should always be rendered AFTER maps since
   character rendering depends on map rendering; map rendering information is used by the
   character rendering routines */

enum pok_character_effect
{
    pok_character_no_effect,
    pok_character_normal_effect,    /* the character moves around in a normal fashion (parameter is dimension) */
    pok_character_jump_effect,      /* the character jumps over a specified number of tiles (parameter is dimension) */
    pok_character_spin_off_effect,  /* the character spins to warp off the map (parameter is acceleration) */
    pok_character_spin_on_effect,   /* the character spins to warp on to the map (parameter is de-acceleration) */
    pok_character_slide_effect      /* the character slides to the next tile (parameter is dimension) */
};

/* pok_character_context: provides information for rendering a single
   'pok_character' object; the context is NOT responsible for freeing its
   associated character */
struct pok_character_context
{
    struct pok_character* character;  /* the character to be drawn */
    struct pok_map* map;              /* cache a reference to the current map */
    struct pok_map_chunk* chunk;      /* cache a reference to the current map chunk */
    uint8_t frame;                    /* which frame is used to render the character (direction is implied here) */
    int offset[2];                    /* x/y offset from current position (in pixels; used for moving sprites/effects) */
    bool_t shadow;                    /* if non-zero, a shadow is drawn on the occupied tile */
    enum pok_character_effect eff;    /* specifies the effect to use */
    uint8_t resolveFrame;             /* resolution frame */
    uint16_t granularity;             /* granularity of animation (how many update cycles does it take to complete?) */
    bool_t slowDown;                  /* takes twice as long to animate (for effect) */
    uint32_t aniTicks;                /* animation ticks up to this point */
    uint32_t aniTicksAmt;             /* number of animation ticks needed before each update */
    uint8_t frameAlt;                 /* sprite frame alternation counter */
    bool_t update;                    /* is the character context being updated? */
};
bool_t pok_character_context_move(struct pok_character_context* context,enum pok_direction direction);
void pok_character_context_set_player(struct pok_character_context* context,struct pok_map_render_context* mapRC);
void pok_character_context_set_update(struct pok_character_context* context,
    enum pok_direction direction,
    enum pok_character_effect effect,
    uint16_t parameter,
    bool_t resetTime);
bool_t pok_character_context_update(struct pok_character_context* context,uint16_t dimension,uint32_t ticks);

/* pok_character_render_context: provides information for rendering a set 
   of 'pok_character_context' instances; this render context depends on the
   map render context to draw characters */
struct pok_character_render_context
{
    /* dynamic array of 'pok_character_context' instances */
    struct dynamic_array chars;

    /* character render context must have reference to map render context and sprite manager */
    const struct pok_map_render_context* mapRC;
    const struct pok_sprite_manager* sman;
};
struct pok_character_render_context* pok_character_render_context_new(const struct pok_map_render_context* mapRC,
    const struct pok_sprite_manager* sman);
void pok_character_render_context_free(struct pok_character_render_context* context);
void pok_character_render_context_init(struct pok_character_render_context* context,const struct pok_map_render_context* mapRC,
    const struct pok_sprite_manager* sman);
void pok_character_render_context_delete(struct pok_character_render_context* context);
bool_t pok_character_render_context_add(struct pok_character_render_context* context,struct pok_character* character);
struct pok_character_context* pok_character_render_context_add_ex(struct pok_character_render_context* context,
    struct pok_character* character);
bool_t pok_character_render_context_remove(struct pok_character_render_context* context,struct pok_character* character);

/* render routine */
void pok_character_render(const struct pok_graphics_subsystem* sys,struct pok_character_render_context* context);

#endif
