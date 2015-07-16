/* config.h - pokgame */
#ifndef POKGAME_CONFIG_H
#define POKGAME_CONFIG_H
#include "types.h"

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

/* system directories: pokgame uses two directories, content and install; the
   install directory houses resources that do not change and do not contain
   user/session specific information; the content directory contains user
   information that the game produces as it goes */
#ifdef POKGAME_DEBUG

/* if we're debugging, just refer to a local 'game' directory */
#define POKGAME_INSTALL_DIRECTORY "game/"
#define POKGAME_CONTENT_DIRECTORY "game/"

#else

#ifdef POKGAME_LINUX

/* linux: the content directory is relative to the current user's home directory; the current
   user name is obtained via the process's environment; the install directory is an absolute
   path under the /usr tree */
#define POKGAME_INSTALL_DIRECTORY "/usr/share/pokgame/"
#define POKGAME_CONTENT_DIRECTORY ".pokgame/"

#elif defined(POKGAME_WIN32)

/* win32: the content directory is relative to the install directory which is
   relative to the user's AppData directory */
#define POKGAME_INSTALL_DIRECTORY "pokgame/"
#define POKGAME_CONTENT_DIRECTORY "pokgame/content/"

#elif defined(POKGAME_OSX)

/* OS X: the content directory is located in the Application Support directory for
   the current user; the install path is determined from the application bundle,
   therefore it does not have any absolute path strings */
#define POKGAME_CONTENT_DIRECTORY "pokgame"

#endif

#endif

#endif
