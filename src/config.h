/* config.h - pokgame */
#ifndef POKGAME_CONFIG_H
#define POKGAME_CONFIG_H
#include "types.h"

/* configure standard error logging */
void configure_stderr();

/* special directory access: these functions return valid directory paths in which
   the game content and install directories can be created */
struct pok_string* pok_get_content_root_path();
struct pok_string* pok_get_install_root_path();

/* pokgame files under the install directory */
#define POKGAME_INSTALL_GLYPHS_FILE "glyphs.png"

/* pokgame files under the content directory */
#define POKGAME_CONTENT_USERSAVE_FILE "user.pokgame"
#define POKGAME_CONTENT_MAPS_FILE     "maps.pokgame"
#define POKGAME_CONTENT_LOG_FILE      "pokgame.log"

/* directories under the install directory */
#define POKGAME_DEFAULT_DIRECTORY "default/"

/* directories under the content directory */
#define POKGAME_VERSION_DIRECTORY "versions/"

/* system directories */
#ifdef POKGAME_DEBUG

#define POKGAME_INSTALL_DIRECTORY "game/"
#define POKGAME_CONTENT_DIRECTORY "game/"

#else

#ifdef POKGAME_LINUX

#define POKGAME_INSTALL_DIRECTORY "/usr/share/pokgame/"
#define POKGAME_CONTENT_DIRECTORY ".pokgame/"

#elif defined(POKGAME_WIN32)

#define POKGAME_INSTALL_DIRECTORY "./" /* on Windows the install directory is relative */
#define POKGAME_CONTENT_DIRECTORY "pokgame/"

#elif defined(POKGAME_OSX)

#endif

#endif

#endif
