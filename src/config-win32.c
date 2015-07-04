/* config-win32.c - pokgame */
#include "config.h"
#include "error.h"
#include <Windows.h>
#include <Shlobj.h>

void configure_stderr()
{
    struct pok_string* path = pok_get_content_root_path();
    pok_string_concat(path,"pokgame.log");

    HANDLE hLog = CreateFile(path->buf,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
    SetStdHandle(STD_ERROR_HANDLE,hLog);

    pok_string_free(path);
}

struct pok_string* pok_get_content_root_path()
{
    DWORD dwAttrib;
    CHAR appData[MAX_PATH];
    struct pok_string* path = pok_string_new_ex(64);
    if (path == NULL)
        pok_error(pok_error_fatal, "memory exception in pok_get_content_root_path()");
#ifndef POKGAME_DEBUG
    /* lookup the application data folder */
    if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appData) != S_OK)
        pok_error(pok_error_fatal, "fail SHGetFolderPath() for content directory path");
    pok_string_assign(path, appData);
    pok_string_concat_char(path, '/');
#endif
    /* append content directory */
    pok_string_concat(path, POKGAME_CONTENT_DIRECTORY);
    /* verify that path exists; create it if not found */
    dwAttrib = GetFileAttributes(path->buf);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            CreateDirectory(path->buf, NULL);
        else
            pok_error(pok_error_fatal, "could not locate content directory");
    }
    else if ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0)
        pok_error(pok_error_fatal, "content directory path does not currently exist as a directory");
    return path;
}

struct pok_string* pok_get_install_root_path()
{
    struct pok_string* path = pok_string_new_ex(64);
    pok_string_assign(path, POKGAME_INSTALL_DIRECTORY);
    return path;
}
