/* config-osx.m - pokgame */
#include "config.h"
#include "error.h"
#import <Foundation/Foundation.h>

/* this file provides configuration functionality for the pokgame engine
   that is specific to the Mac OS X platform */

struct pok_string* pok_get_content_root_path()
{
    /* obtain the content path from the special Library/Application Support
       directory defined in the NSSearchPathDirectory enumeration */
    struct pok_string* path = pok_string_new();
    if (path == NULL) {
        pok_error(pok_error_fatal,"memory exception in pok_get_content_root_path()");
        return NULL;
    }
#ifndef POKGAME_DEBUG
    pok_string_assign(path,[[[[NSFileManager defaultManager]
                      URLForDirectory:NSApplicationSupportDirectory
                          inDomain:NSUserDomainMask
                              appropriateForURL:nil
                                create:YES
                                    error:nil] path] UTF8String]);
    pok_string_concat_char(path,'/');
#endif
    pok_string_concat(path,POKGAME_CONTENT_DIRECTORY);
    return path;
}

struct pok_string* pok_get_install_root_path()
{
    struct pok_string* path = pok_string_new();
    if (path == NULL) {
        pok_error(pok_error_fatal,"memory exception in pok_get_install_root_path()");
        return NULL;
    }
#ifdef POKGAME_DEBUG
    /* note: on OS X the symbol POKGAME_INSTALL_DIRECTORY is only defined if
       POKGAME_DEBUG is defined */
    pok_string_assign(path,POKGAME_INSTALL_DIRECTORY);
#else
    /* otherwise obtain the install path from the application bundle path */
    NSString* bundlePath = [NSBundle mainBundle].bundlePath;
    pok_string_assign(path,[bundlePath UTF8String]);
    
#endif
    return path;
}
