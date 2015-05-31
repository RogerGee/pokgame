/* update-proc.c - pokgame */
#include "pokgame.h"
#include "error.h"

static void update_key_input(struct pok_game_info* info);

int update_proc(struct pok_game_info* info)
{

    /* counters */
    uint64_t scrollTicks = 0;

    /* game logic loop */
    do {
        /* key input logic */
        update_key_input(info);

    } while (TRUE);

    return 0;
}

void update_key_input(struct pok_game_info* info)
{
}
