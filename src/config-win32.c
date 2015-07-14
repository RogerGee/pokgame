/* config-win32.c - pokgame */
#include "config.h"
#include "error.h"
#include <Windows.h>
#include <Shlobj.h>

static void assign_app_data_path(struct pok_string* path)
{
    CHAR appData[MAX_PATH];
    /* lookup the application data folder */
    if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appData) != S_OK)
        pok_error(pok_error_fatal, "fail SHGetFolderPath() for content directory path");
    pok_string_assign(path, appData);
    pok_string_concat_char(path, '/');
}

struct pok_string* pok_get_content_root_path()
{
    DWORD dwAttrib;
    struct pok_string* path = pok_string_new_ex(64);
    if (path == NULL)
        pok_error(pok_error_fatal, "memory exception in pok_get_content_root_path()");
#ifndef POKGAME_DEBUG
    assign_app_data_path(path);
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
    /* the install directory is relative to the user's AppData directory; we do
       not have to verify that it exists or create it */
    struct pok_string* path = pok_string_new_ex(64);
#ifndef POKGAME_DEBUG
    assign_app_data_path(path);
#endif
    pok_string_concat(path, POKGAME_INSTALL_DIRECTORY);
    return path;
}
