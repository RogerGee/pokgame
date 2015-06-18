/* io-proc.c - pokgame */
#include "pokgame.h"
#include "error.h"

/* the IO procedure controls the flow of game state in the program; data flow could
   exist within this process or with a remote peer; the IO procedure must make sure
   it obtains a readers'/writer lock when reading or writing to the game state; the
   IO procedure does not control the render procedure; the graphics subsystem is
   handled by the caller; the IO procedure is however responsible for the update thread
   which it spawns on the version it runs; */


/* functions */
static void start_update(struct pok_game_info* game);
static void join_update(struct pok_game_info* game);

int io_proc(struct pok_game_info* defaultGame)
{
    do {
        if (defaultGame->versionCBack != NULL) {
            start_update(defaultGame);
            (*defaultGame->versionCBack)(defaultGame);
            join_update(defaultGame);
        }



        pok_timeout(&defaultGame->ioTimeout);
    } while ( pok_graphics_subsystem_has_window(defaultGame->sys) );


    return 0;
}

void start_update(struct pok_game_info* game)
{
}

void join_update(struct pok_game_info* game)
{
}
