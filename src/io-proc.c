/* io-proc.c - pokgame */
#include "pokgame.h"
#include "error.h"

/* the IO procedure handles communication with a remote/local peer (or in the case of
   the default version, with this process itself); the IO procedure also handles updates,
   which can occur spontaneously; this procedure must perform mutual exclusion when modifying
   of accessing a member of the 'pok_game_info' structure */

int io_proc(struct pok_game_info* info)
{

    return 0;
}