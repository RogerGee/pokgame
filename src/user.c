/* user.c - pokgame */
#include "user.h"
#include "net.h"
#include "error.h"
#include "config.h"
#include "pok-stdenum.h"
#include <stdlib.h>

/* the program only handles one user at a time, so allocate a global user info
   structure in the data segment; this structure is loaded when the program begins
   and saved when it quits */
static bool_t loaded = FALSE;
static struct pok_user_info userInfo;

static struct pok_string* get_user_save_file_path()
{
    struct pok_string* path = pok_get_content_root_path();
    pok_string_concat(path,POKGAME_CONTENT_USERSAVE_FILE);
    return path;
}

static void load_default_settings()
{
    int i;
    char guid[GUID_LENGTH];
    /* mark the game as new so the caller can prompt the user
       to specify user info settings later */
    userInfo.new = TRUE;
    /* randomly generate guid */
    for (i = 0;i < GUID_LENGTH;++i)
        guid[i] = rand();
    pok_string_assign_ex(&userInfo.guid,guid,GUID_LENGTH);
    pok_string_assign(&userInfo.name,"player");
    userInfo.sprite = pok_std_sprite_male_player;
    userInfo.gender = pok_gender_male;
}

void pok_user_load_module()
{
    size_t sz;
    struct pok_string* path;
    struct pok_data_source* fin;
    /* initialize user info members */
    pok_string_init_ex(&userInfo.guid,GUID_LENGTH);
    pok_string_init(&userInfo.name);
    path = get_user_save_file_path();
    fin = pok_data_source_new_file(path->buf,pok_filemode_open_existing,pok_iomode_read);
    if (fin == NULL) {
        if ( pok_exception_peek_ex(pok_ex_net,pok_ex_net_file_does_not_exist) ) {
            /* file does not exist, so assign default settings */
            load_default_settings();
            pok_exception_pop();
        }
        else
            pok_error_fromstack(pok_error_fatal);
    }
    else {
        /* read settings from file*/
        userInfo.new = FALSE;
        pok_data_source_read_to_buffer(fin,userInfo.guid.buf,GUID_LENGTH,&sz);
        if (sz != GUID_LENGTH)
            goto fail;
        if (!pok_data_stream_read_string_ex(fin,&userInfo.name)
            || !pok_data_stream_read_uint16(fin,&userInfo.sprite)
            || !pok_data_stream_read_byte(fin,&userInfo.gender))
            goto fail;
        pok_data_source_free(fin);
    }
    loaded = TRUE;
    return;
fail:
    load_default_settings();
    loaded = TRUE;
}
void pok_user_unload_module()
{
    /* just save the user info structure; mark as unloaded so we don't return
       the structure uninitialized */
    pok_user_save();
    loaded = FALSE;
    /* delete members */
    pok_string_delete(&userInfo.guid);
    pok_string_delete(&userInfo.name);
}

struct pok_user_info* pok_user_get_info()
{
    /* return the user info structure; if this module is not loaded, then 
       return NULL; the calling functionality should be able to cope without
       user info */
    if (loaded)
        return &userInfo;
    return NULL;
}
void pok_user_save()
{
    size_t sz;
    struct pok_string* path;
    struct pok_data_source* fout;
    path = get_user_save_file_path();
    fout = pok_data_source_new_file(path->buf,pok_filemode_create_always,pok_iomode_write);
    if (fout == NULL) {
        pok_error_fromstack(pok_error_warning);
        return;
    }
    pok_data_source_write(fout,(byte_t*)userInfo.guid.buf,GUID_LENGTH,&sz);
    pok_data_stream_write_string(fout,userInfo.name.buf);
    pok_data_stream_write_uint16(fout,userInfo.sprite);
    pok_data_stream_write_byte(fout,userInfo.gender);
    pok_data_source_free(fout);
}
