/* character.c - pokgame */
#include "character.h"
#include "error.h"
#include <stdlib.h>

/* pok_character */
struct pok_character* pok_character_new()
{
    struct pok_character* character;
    character = malloc(sizeof(struct pok_character));
    if (character == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_netobj_default_ex(&character->_base,pok_netobj_character);
    character->spriteIndex = 0;
    character->direction = pok_direction_down;
    character->mapNo = 0;
    character->chunkPos.X = 0;
    character->chunkPos.Y = 0;
    character->tilePos.column = 0;
    character->tilePos.row = 0;
    character->isPlayer = FALSE;
    return character;
}
struct pok_character* pok_character_new_ex(uint16_t spriteIndex,uint32_t mapNo,
    struct pok_point* chunkPos,struct pok_location* tilePos)
{
    struct pok_character* character;
    character = malloc(sizeof(struct pok_character));
    if (character == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_netobj_default_ex(&character->_base,pok_netobj_character);
    character->spriteIndex = spriteIndex;
    character->direction = pok_direction_down;
    character->mapNo = mapNo;
    character->chunkPos = *chunkPos;
    character->tilePos = *tilePos;
    character->isPlayer = FALSE;
    return character;
}
void pok_character_free(struct pok_character* character)
{
    free(character);
}
