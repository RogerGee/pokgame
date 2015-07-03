/* error.c - pokgame */
#include "error.h"
#include <dstructs/hashmap.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern const char* POKGAME_NAME;

static int pok_get_thread_id();
static void pok_lock_error_module();
static void pok_unlock_error_module();

/* include target-specific code */
#if defined(POKGAME_POSIX)
#include "error-posix.c"
#elif defined(POKGAME_WIN32)
#include "error-win32.c"
#endif

/* error messages: each array of arrays represents a collection of error messages
   based on exception kind; these messages are meant to be seen by the user */
static char const* const* POK_ERROR_MESSAGES[] = {
    (const char* []) { /* pok_ex_default */
        "an undocumented error occurred",
        "a memory allocation failed"
    },
    (const char* []) { /* pok_ex_net */
        "an unspecified IO error occurred", /* pok_ex_net_unspec */
        "an IO operation was interrupted", /* pok_ex_net_interrupt */
        "an IO operation would have blocked", /* pok_ex_net_wouldblock */
        "an IO operation did not return all the data requested", /* pok_ex_net_pending */
        "an IO operation wrote to a broken pipe", /* pok_ex_net_brokenpipe */
        "an IO operation flagged end of communications", /* pok_ex_net_endofcomms */
        "the output buffer is full", /* pok_ex_net_noroom */
        "specified file does not exist", /* pok_ex_net_file_does_not_exist */
        "specified file already exists", /* pok_ex_net_file_already_exist */
        "cannot open specified file: permission denied", /* pok_ex_net_file_permission_denied */
        "the file path is incorrect" /* pok_ex_net_file_bad_path */
        "couldn't create IO device (local)", /* pok_ex_net_could_not_create_local */
        "couldn't create IO device (named local)", /* pok_ex_net_could_not_create_named_local */
        "couldn't create IO device (remote)", /* pok_ex_net_could_not_create_remote */
        "cannot create process", /* pok_ex_net_could_not_create_process */
        "cannot execute program", /* pok_ex_net_bad_program */
        "cannot execute program: file does not exist", /* pok_ex_net_program_not_found */
        "cannot execute program: permission denied" /* pok_ex_net_execute_denied */
    },
    (const char* []) { /* pok_ex_netobj */
        "the specified network id was already allocated" /* pok_ex_netobj_bad_id */
    },
    (const char* []) { /* pok_ex_graphics */
        "a bad dimension was specified to the graphics subsystem", /* pok_ex_graphics_bad_dimension */
        "a bad window size was specified to the graphics subsystem", /* pok_ex_graphics_bad_window_size */
        "a bad player location was specified to the graphics subsystem", /* pok_ex_graphics_bad_player_location */
        "a bad player offset was specified to the graphics subsystem", /* pok_ex_graphics_bad_player_offset */
        "the graphics subsystem was already started" /* pok_ex_graphics_already_started */
    },
    (const char* []) { /* pok_ex_image */
        "the image format is unrecognized", /* pok_ex_image_unrecognized_format */
        "the image's color format is not supported", /* pok_ex_image_bad_color_format */
        "the specified image was too large", /* pok_ex_image_too_big */
        "the specified subimage was invalid", /* pok_ex_image_invalid_subimage */
        "the image object was already loaded", /* pok_ex_image_already_loaded */
        "the image being read from file was incomplete" /* pok_ex_image_incomplete_fromfile */
    },
    (const char* []) { /* pok_ex_tileman */
        "a zero amount of tiles was specified", /* pok_ex_tileman_zero_tiles */
        "too few tile animation parameters were specified", /* pok_ex_tileman_too_few_ani */
        "the tile manager was already configured for the specified operation", /* pok_ex_tileman_already */
        "the specified tile sheet dimensions were incorrect" /* pok_ex_tileman_bad_image_dimension */
    },
    (const char* []) { /* pok_ex_spriteman */
        "the sprite manager was already configured for the specified operation", /* pok_ex_spriteman_already */
        "the specified sprite sheet dimensions were incorrect" /* pok_ex_spriteman_bad_image_dimension */
    },
    (const char* []) { /* pok_ex_tile */
        "a bad tile warp kind parameter was specified" /* pok_ex_tile_bad_warp_kind */
    },
    (const char* []) { /* pok_ex_map */
        "the specified chunk size was incorrect", /* pok_ex_map_bad_chunk_size */
        "a zero amount of chunks was specified", /* pok_ex_map_zero_chunks */
        "the operation could not complete because the map was already loaded", /* pok_ex_map_already */
        "the operation could not complete because the map was not loaded", /* pok_ex_map_not_loaded */
        "map information was incorrect", /* pok_ex_map_bad_format */
        "a new chunk was created at an already allocated position on the map" /* pok_ex_map_non_unique_chunk */
    }
};

/* target-independent code */

void pok_error(enum pok_errorkind kind,const char* message, ...)
{
    va_list args;
    char finalMessage[4096];
    va_start(args,message);
    vsnprintf(finalMessage,sizeof(finalMessage),message,args);
    va_end(args);
    if (kind == pok_error_warning)
        fprintf(stderr,"%s: warning: %s\n",POKGAME_NAME,finalMessage);
    else if (kind == pok_error_unimplemented)
        fprintf(stderr,"%s: unimplemented: %s\n",POKGAME_NAME,finalMessage);
    else if (kind == pok_error_fatal) {
        fprintf(stderr,"%s: fatal error: %s\n",POKGAME_NAME,finalMessage);
        exit(EXIT_FAILURE);
    }
}
void pok_error_fromstack(enum pok_errorkind kind)
{
    const struct pok_exception* ex;
    ex = pok_exception_pop();
    if (ex != NULL) {
        if (kind == pok_error_warning)
            fprintf(stderr,"%s: warning: %s\n",POKGAME_NAME,ex->message);
        else if (kind == pok_error_unimplemented)
            fprintf(stderr,"%s: unimplemented: %s\n",POKGAME_NAME,ex->message);
        else if (kind == pok_error_fatal)
            fprintf(stderr,"%s: fatal error: %s\n",POKGAME_NAME,ex->message);
    }
    else
        fprintf(stderr,"%s: error stack was empty!\n",POKGAME_NAME);
    if (kind == pok_error_fatal)
        exit(EXIT_FAILURE);
}

/* pok exception handling; record exceptions on a per-thread basis */

struct thread_exception_item
{
    int id; /* key: must be first value in struct; this is the thread id */
    bool_t popped; /* non-zero if the exception has been popped */
    struct pok_exception* ex; /* the exception */
};
static void thread_exception_item_init(struct thread_exception_item* item,int id,struct pok_exception* ex)
{
    item->id = id;
    item->popped = FALSE;
    item->ex = ex;
}
static void thread_exception_item_dstor(struct thread_exception_item* item)
{
    free(item->ex);
    free(item);
}

/* pok_exception */
static struct hashmap exceptions;
static bool_t init = FALSE;
static bool_t memory_error_flag = FALSE;
/* this must be stored in the program's data segment; if memory does run out, we
   need to be able to still create this structure */
static struct pok_exception memerror = {pok_ex_default_memory_allocation_fail,
                                        pok_ex_default,
                                        "memory allocation exception"};

static void add_exception(struct pok_exception* ex)
{
    int tid;
    struct thread_exception_item* list;
    /* place exception in data structure */
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        /* create new structure for thread */
        list = malloc(sizeof(struct thread_exception_item));
        thread_exception_item_init(list,tid,ex);
        hashmap_insert(&exceptions,list);
    }
    else {
#ifdef POKGAME_DEBUG
        if (!list->popped)
            pok_error(pok_error_warning,"exception module discarded unpopped exception");
#endif
        free(list->ex);
        list->ex = ex;
        list->popped = FALSE;
    }
    pok_unlock_error_module();
}

void pok_exception_load_module()
{
#ifdef POKGAME_DEBUG
    if (init)
        pok_error(pok_error_fatal,"module 'exception' was already loaded");
#endif
    pok_lock_error_module();
    hashmap_init(&exceptions,25,(hash_function)hash_int,(key_comparator)intcmp);
    init = TRUE;
    pok_unlock_error_module();
}
void pok_exception_flag_memory_error()
{
    /* memory error is a global setting; any thread may pick it up at random; we
       don't really care which thread picks it up since (most likely) the program
       will decide to terminate at that point */
    memory_error_flag = TRUE;
}
void pok_exception_unload_module()
{
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    pok_lock_error_module();
    hashmap_delete_ex(&exceptions,(destructor)thread_exception_item_dstor);
    init = FALSE;
    pok_unlock_error_module();
}
struct pok_exception* pok_exception_new()
{
    struct pok_exception* ex;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    ex = malloc(sizeof(struct pok_exception));
    ex->id = -1;
    ex->kind = pok_ex_default;
    ex->message[0] = 0;
    add_exception(ex);
    return ex;
}
struct pok_exception* pok_exception_new_ex(enum pok_ex_kind kind,int id)
{
    int tid;
    struct thread_exception_item* list;
    struct pok_exception* ex;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    ex = malloc(sizeof(struct pok_exception));
    ex->id = id;
    ex->kind = kind;
    strncpy(ex->message,POK_ERROR_MESSAGES[kind][id],sizeof(ex->message));
    add_exception(ex);
    return ex;
}
struct pok_exception* pok_exception_new_format(const char* message, ...)
{
    int tid;
    va_list args;
    struct thread_exception_item* list;
    struct pok_exception* ex;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    ex = malloc(sizeof(struct pok_exception));
    ex->id = pok_ex_default_undocumented;
    ex->kind = pok_ex_default;
    va_start(args,message);
    vsnprintf(ex->message,sizeof(ex->message),message,args);
    va_end(args);
    add_exception(ex);
    return ex;
}
bool_t pok_exception_check()
{
    /* is there an exception for the current thread? */
    int tid;
    bool_t result;
    struct thread_exception_item* list;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    if (memory_error_flag)
        return TRUE;
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    /* there is an exception if there is a structure whose exception has
       not been popped */
    result = !(list==NULL || list->popped);
    pok_unlock_error_module();
    return result;
}
bool_t pok_exception_check_ex(enum pok_ex_kind kind,int id)
{
    /* is there an exception of the specified kind/id on the thread-specific stack? */
    size_t i;
    int tid;
    bool_t result;
    struct thread_exception_item* list;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    if (memory_error_flag)
        return TRUE;
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    result = list!=NULL && !list->popped && list->ex->kind == kind && list->ex->id == id;
    pok_unlock_error_module();
    return result;
}
const struct pok_exception* pok_exception_pop()
{
    /* return exception on current thread (if any); NULL is returned
       if no exception has been added; it will remain valid so long
       as another exception is not created for the thread */
    int tid;
    struct pok_exception* ex;
    struct thread_exception_item* list;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    /* return the memory error structure's address; this structure is allocated
       within the program's data segment so that we can use it if memory is exhausted */
    if (memory_error_flag) {
        memory_error_flag = FALSE;
        return &memerror;
    }
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list==NULL || list->popped) {
        pok_unlock_error_module();
        return NULL;
    }
    ex = list->ex;
    list->popped = TRUE;
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_pop_ex(enum pok_ex_kind kind,int id)
{
    /* see if the next exception is of 'kind' and 'id'; if so pop it off */
    int tid;
    struct pok_exception* ex;
    struct thread_exception_item* list;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    if (memory_error_flag) {
        memory_error_flag = FALSE;
        return &memerror;
    }
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list==NULL || list->popped || list->ex->kind != kind || list->ex->id != id) {
        pok_unlock_error_module();
        return NULL;
    }
    ex = list->ex;
    list->popped = TRUE;
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_peek()
{
    /* return top-most exception on thread-specific stack but do not
       mark it for removal */
    int tid;
    struct pok_exception* ex;
    struct thread_exception_item* list;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    if (memory_error_flag)
        return &memerror;
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    ex = list != NULL && !list->popped ? list->ex : NULL;
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_peek_ex(enum pok_ex_kind kind,int id)
{
    /* return top-most exception of specified kind on thread-specific stack but do not
       mark it for removal; if the current exception is not of 'kind' then NULL is returned */
    int tid;
    struct pok_exception* ex;
    struct thread_exception_item* list;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    if (memory_error_flag)
        return &memerror;
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    ex = list != NULL && !list->popped && list->ex->kind == kind && list->ex->id == id ? list->ex : NULL;
    pok_unlock_error_module();
    return ex;
}
