/* character.h - pokgame */
#ifndef POKGAME_CHARACTER_H
#define POKGAME_CHARACTER_H
#include "netobj.h"

/* pok_character: */
struct pok_character
{
    struct pok_netobj _base;

    /* general properties */
    uint16_t spriteIndex;         /* index defining which set of sprite images to use */
    enum pok_direction direction; /* logical direction that the character faces */
    uint32_t mapNo;               /* determines which map occupies the character */
    struct pok_point chunkPos;    /* position of chunk containing character within map */
    struct pok_location tilePos;  /* position within chunk (tile position) */
    bool_t isPlayer;              /* flag whether the character is a human player (user or remote) */

};
struct pok_character* pok_character_new();
struct pok_character* pok_character_new_ex(uint16_t spriteIndex,uint32_t mapNo,
    struct pok_point* chunkPos,struct pok_location* tilePos);
void pok_character_free(struct pok_character* character);

#endif
