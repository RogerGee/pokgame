/* error.c - pokgame */
#include "error.h"
#include <dstructs/hashmap.h>
#include <dstructs/stack.h>
#include <stdlib.h>
#include <stdio.h>

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
    (const char* []) { /* pok_ex_graphics */
        "a bad dimension was specified to the graphics subsystem", /* pok_ex_graphics_bad_dimension */
        "a bad window size was specified to the graphics subsystem", /* pok_ex_graphics_bad_window_size */
        "a bad player location was specified to the graphics subsystem", /* pok_ex_graphics_bad_player_location */
        "a bad player offset was specified to the graphics subsystem" /* pok_ex_graphics_bad_player_offset */
    },
    (const char* []) { /* pok_ex_image */
        "the image format is unrecognized", /* pok_ex_image_unrecognized_format */
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
        "map information was incorrect", /* pok_ex_map_bad_format */
        "a new chunk was created at an already allocated position on the map" /* pok_ex_map_non_unique_chunk */
    }
};

/* target-independent code */

void pok_error(enum pok_errorkind kind,const char* message)
{
    pok_lock_error_module();
    if (kind == pok_error_warning)
        fprintf(stderr,"%s: warning: %s\n",POKGAME_NAME,message);
    else if (kind == pok_error_unimplemented)
        fprintf(stderr,"%s: unimplemented: %s\n",POKGAME_NAME,message);
    else if (kind == pok_error_fatal) {
        fprintf(stderr,"%s: fatal error: %s\n",POKGAME_NAME,message);
        exit(EXIT_FAILURE);
    }
    pok_unlock_error_module();
}
void pok_error_fromstack(enum pok_errorkind kind)
{
    const struct pok_exception* ex;
    ex = pok_exception_pop();
    if (ex != NULL) {
        pok_lock_error_module();
        if (kind == pok_error_warning)
            fprintf(stderr,"%s: warning: %s",POKGAME_NAME,ex->message);
        else if (kind == pok_error_unimplemented)
            fprintf(stderr,"%s: unimplemented: %s",POKGAME_NAME,ex->message);
        else if (kind == pok_error_fatal)
            fprintf(stderr,"%s: fatal error: %s",POKGAME_NAME,ex->message);
        pok_unlock_error_module();
        if (ex->lineno != 0)
            fprintf(stderr,": line %d\n",ex->lineno);
        else
            fputc('\n',stderr);
    }
    else
        fprintf(stderr,"%s: error stack was empty!\n",POKGAME_NAME);
    if (kind == pok_error_fatal)
        exit(EXIT_FAILURE);
}

/* pok exception handling; record exceptions on a per-thread basis */

struct thread_exception_item
{
    int id; /* key: must be first value in struct */
    bool_t last; /* does last value need to be popped off 'exceptions'? */
    struct stack exceptions;
};
static void thread_exception_item_init(struct thread_exception_item* item,int id)
{
    item->id = id;
    item->last = FALSE;
    stack_init(&item->exceptions,free);
}
static void thread_exception_item_dstor(struct thread_exception_item* item)
{
    stack_delete(&item->exceptions);
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
                                        "memory allocation exception",
                                        0};

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
    int tid;
    struct thread_exception_item* list;
    struct pok_exception* ex;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    ex = malloc(sizeof(struct pok_exception));
    ex->id = -1;
    ex->lineno = 0;
    ex->kind = pok_ex_default;
    ex->message = NULL;
    /* place exceptions in data structure */
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        list = malloc(sizeof(struct thread_exception_item));
        thread_exception_item_init(list,tid);
        hashmap_insert(&exceptions,list);
    }
    stack_push(&list->exceptions,ex);
    list->last = FALSE; /* current top of stack is current exception */
    pok_unlock_error_module();
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
    ex->lineno = 0;
    ex->kind = kind;
    ex->message = POK_ERROR_MESSAGES[kind][id];
    /* place exceptions in data structure */
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        list = malloc(sizeof(struct thread_exception_item));
        thread_exception_item_init(list,tid);
        hashmap_insert(&exceptions,list);
    }
    stack_push(&list->exceptions,ex);
    list->last = FALSE; /* current top of stack is current exception */
    pok_unlock_error_module();
    return ex;
}
struct pok_exception* pok_exception_new_ex2(int lineno,const char* message)
{
    int tid;
    struct thread_exception_item* list;
    struct pok_exception* ex;
#ifdef POKGAME_DEBUG
    if (!init)
        pok_error(pok_error_fatal,"module 'exception' was unloaded");
#endif
    ex = malloc(sizeof(struct pok_exception));
    ex->id = pok_ex_default;
    ex->lineno = lineno;
    ex->kind = pok_ex_default_undocumented;
    ex->message = message;
    /* place exceptions in data structure */
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        list = malloc(sizeof(struct thread_exception_item));
        thread_exception_item_init(list,tid);
        hashmap_insert(&exceptions,list);
    }
    stack_push(&list->exceptions,ex);
    list->last = FALSE; /* current top of stack is current exception */
    pok_unlock_error_module();
    return ex;
}
bool_t pok_exception_check()
{
    /* is there an exception on the thread-specific stack? */
    int tid;
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
    if (list==NULL || stack_is_empty(&list->exceptions)) {
        pok_unlock_error_module();
        return FALSE;
    }
    pok_unlock_error_module();
    return TRUE;
}
bool_t pok_exception_check_ex(enum pok_ex_kind kind)
{
    /* is there an exception of the specified kind on the thread-specific stack? */
    size_t i;
    int tid;
    void** buf;
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
    if (list!=NULL && !stack_is_empty(&list->exceptions)) {
        for (i = 0,buf = stack_getbuffer(&list->exceptions);i < stack_getsize(&list->exceptions);++i) {
            if (((struct pok_exception*)buf[i])->kind == kind) {
                pok_unlock_error_module();
                return TRUE;
            }
        }
    }
    pok_unlock_error_module();
    return FALSE;
}
const struct pok_exception* pok_exception_pop()
{
    /* return top-most exception on thread-specific stack; it will
       remain valid so long as the user doesn't make another call
       to a pop_exception_pop*() function variant */
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
    if (list==NULL || stack_is_empty(&list->exceptions)) {
        pok_unlock_error_module();
        return NULL;
    }
    if (list->last)
        stack_pop(&list->exceptions);
    ex = stack_top(&list->exceptions);
    list->last = TRUE;
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_pop_ex(enum pok_ex_kind kind)
{
    /* see if the next exception is of 'kind' and if so pop it off */
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
    if (list==NULL || stack_is_empty(&list->exceptions) || 
        ((struct pok_exception*)stack_top(&list->exceptions))->kind!=kind) {
        pok_unlock_error_module();
        return NULL;
    }
    if (list->last)
        stack_pop(&list->exceptions);
    ex = stack_top(&list->exceptions);
    list->last = TRUE;
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
    ex = NULL;
    if (list != NULL) {
        if (list->last)
            stack_pop(&list->exceptions);
        ex = stack_top(&list->exceptions); /* will be NULL if stack is empty */
    }
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_peek_ex(enum pok_ex_kind kind)
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
    ex = NULL;
    if (list != NULL) {
        if (list->last)
            stack_pop(&list->exceptions);
        ex = stack_top(&list->exceptions);
        if (ex->kind != kind)
            ex = NULL;
    }
    pok_unlock_error_module();
    return ex;
}
