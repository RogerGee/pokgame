/* net.c - pokgame */
#include "net.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

/* include target-specific code */
#if defined(POKGAME_POSIX)
#include "net-posix.c"
#elif defined(POKGAME_WIN32)

#endif

/* target-independent code */
