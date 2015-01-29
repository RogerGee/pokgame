/* error.c - pokgame */
#include "error.h"
#include <dstructs/hashmap.h>
#include <dstructs/stack.h>
#include <stdlib.h>
#include <stdio.h>

extern const char* POKGAME_NAME;

static inline int pok_get_thread_id();
static void pok_lock_error_module();
static void pok_unlock_error_module();

/* include target-specific code */
#if defined(POKGAME_POSIX)
#include "error-posix.c"
#elif defined(POKGAME_WIN32)

#endif

/* target-independent code */

void pok_error(enum pok_errorkind kind,const char* message)
{
    pok_lock_error_module();
    if (kind == pok_error_warning)
        fprintf(stderr,"%s: warning: %s\n",POKGAME_NAME,message);
    else if (kind == pok_error_unimplemented)
        fprintf(stderr,"%s: unimplemented: %s",POKGAME_NAME,message);
    else if (kind == pok_error_fatal) {
        fprintf(stderr,"%s: fatal error: %s\n",POKGAME_NAME,message);
        exit(EXIT_FAILURE);
    }
    pok_unlock_error_module();
}

/* pok exception handling; record exceptions on a per-thread basis */

struct thread_exception_item
{
    int id; /* key */
    struct stack exceptions;
    struct pok_exception* last;
};
static void thread_exception_item_dstor(struct thread_exception_item* item)
{
    stack_delete_ex(&item->exceptions,free);
    free(item->last);
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

void pok_exception_load()
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
    /* memory error is a global setting; any thread may pick it up at random */
    memory_error_flag = TRUE;
}
void pok_exception_unload()
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
    ex->kind = pok_ex_default;
    ex->message = NULL;
    /* place exceptions in data structure */
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        list = malloc(sizeof(struct thread_exception_item));
        list->id = tid;
        list->last = NULL;
        stack_init(&list->exceptions);
        hashmap_insert(&exceptions,list);
    }
    stack_push(&list->exceptions,ex);
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
    ex->kind = kind;
    ex->message = NULL;
    /* place exceptions in data structure */
    pok_lock_error_module();
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        list = malloc(sizeof(struct thread_exception_item));
        list->id = tid;
        list->last = NULL;
        stack_init(&list->exceptions);
        hashmap_insert(&exceptions,list);
    }
    stack_push(&list->exceptions,ex);
    pok_unlock_error_module();
    return ex;
}
bool_t pok_exception_check()
{
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
    int i;
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
    ex = stack_pop(&list->exceptions);
    if (list->last != NULL)
        free(list->last);
    list->last = ex;
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_pop_ex(enum pok_ex_kind kind)
{
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
    ex = stack_pop(&list->exceptions);
    if (list->last != NULL)
        free(list->last);
    list->last = ex;
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_peek()
{
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
    if (list!=NULL && !stack_is_empty(&list->exceptions))
        ex = stack_top(&list->exceptions);
    pok_unlock_error_module();
    return ex;
}
const struct pok_exception* pok_exception_peek_ex(enum pok_ex_kind kind)
{
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
    if (list!=NULL && !stack_is_empty(&list->exceptions)) {
        ex = stack_top(&list->exceptions);
        if (ex->kind != kind)
            ex = NULL;
    }
    pok_unlock_error_module();
    return ex;
}
