/* gamelock.h - pokgame */
#ifndef POKGAME_GAMELOCK_H
#include "types.h"

/* module load/unload */
void pok_gamelock_load_module();
void pok_gamelock_unload_module();

/* timeout_interval: structure to represent game timeouts */
struct pok_timeout_interval
{
    /* timeout duration */
    uint32_t mseconds;
    uint32_t useconds;
    uint32_t nseconds;

    /* how many ticks actually elapsed since the last timeout;
       a tick is defined as a single millisecond */
    uint32_t elapsed;
    uint64_t _before[2];
};
void pok_timeout_interval_reset(struct pok_timeout_interval* t,uint32_t mseconds);
void pok_timeout(struct pok_timeout_interval* interval);
void pok_timeout_no_elapsed(struct pok_timeout_interval* interval);
void pok_timeout_grab_counter(struct pok_timeout_interval* interval);
void pok_timeout_calc_elapsed(struct pok_timeout_interval* interval);

/* these functions provide mutual exclusion when an object is edited; the 'modify' functions
   should be called to ensure code may modify the specified object undisturbed; if the code
   need only read an object, then the 'lock' function should be called; these functions block
   the thread if necessary */
void pok_game_modify_enter(void* object); /* enter modify context */
void pok_game_modify_exit(void* object); /* exit modify context */
void pok_game_lock(void* object); /* ensure that 'object' is not being modified (read-only access) */
void pok_game_unlock(void* object); /* exit 'lock' context (read-only access) */

/* gamelock: represents a lock on a particular game object */

struct gamelock;
struct gamelock* gamelock_new(void* object);
void gamelock_free(struct gamelock* lock);
void gamelock_aquire(struct gamelock* lock);
void gamelock_release(struct gamelock* lock);
void gamelock_up(struct gamelock* lock);
void gamelock_down(struct gamelock* lock);
int gamelock_compar(const struct gamelock* left,const struct gamelock* right);

#endif
