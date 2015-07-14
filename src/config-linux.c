/* config-linux.c - pokgame */
#include "config.h"
#include "error.h"
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* this file provides functionality for the pokgame engine that is specific to the linux platform */

struct pok_string* pok_get_content_root_path()
{
    const char* username;
    struct passwd* passwd;
    struct stat statbuf;
    struct pok_string* path = pok_string_new_ex(64);
    if (path == NULL)
        pok_error(pok_error_fatal,"memory exception in pok_get_content_root_path()");
#ifndef POKGAME_DEBUG
    /* obtain user name from environment */
    username = getenv("USER");
    if (username == NULL)
        pok_error(pok_error_fatal,"fail getenv() for variable USER: %s",strerror(errno));
    /* obtain home directory from system password file; caution: this version
       is not thread safe (pokgame should only call it from one thread or in a
       single-threaded context) */
    passwd = getpwnam(username);
    pok_string_assign(path,passwd->pw_dir);
    pok_string_concat_char(path,'/');
#endif
    /* append content directory */
    pok_string_concat(path,POKGAME_CONTENT_DIRECTORY);
    /* make sure the path exists, if not then create it */
    if (stat(path->buf,&statbuf) == -1) {
        if (errno == ENOENT) {
            /* doesn't exist; try to create the top level directory */
            if (mkdir(path->buf,0666) == -1)
                pok_error(pok_error_fatal,"failed to create content directory: %s",strerror(errno));
        }
        else
            pok_error(pok_error_fatal,"couldn't find suitable content directory: %s",strerror(errno));
    }
    else if ( !S_ISDIR(statbuf.st_mode) )
        pok_error(pok_error_fatal,"content directory exists as something other than a directory!");
    return path;
}

struct pok_string* pok_get_install_root_path()
{
    /* create string with install directory path; do not attempt to create
       the path if it does not exist: it should already exist */
    struct pok_string* path = pok_string_new_ex(64);
    if (path == NULL) {
        pok_error(pok_error_fatal,"memory exception in pok_get_install_root_path()");
        return NULL;
    }
    pok_string_assign(path,POKGAME_INSTALL_DIRECTORY);
    return path;
}
