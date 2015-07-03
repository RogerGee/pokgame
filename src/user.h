/* user.h - pokgame */
#ifndef POKGAME_USER_H
#define POKGAME_USER_H
#include "types.h"

void pok_user_load_module();
void pok_user_unload_module();

/* pok_user_info: stores information related to a single user instance of the application */
struct pok_user_info
{
    bool_t new;                  /* if non-zero, the user info was just generated (user started new game) */
    struct pok_string guid;      /* 128-bit guid stored in ASCII */
    struct pok_string name;      /* player name string (this is copied over into the player character) */
    uint16_t sprite;             /* the sprite set to use for the player (player gets to choose this when starting a new game) */
    uint8_t gender;              /* gender (the player gets to pick this; it doesn't have to match the sprite artwork) */

};

struct pok_user_info* pok_user_get_info();
void pok_user_save();

#endif

