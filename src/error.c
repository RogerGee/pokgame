/* error.c - pokgame */
#include "error.h"
#include <dstructs/hashmap.h>
#include <dstructs/stack.h>
#include <stdlib.h>
#include <stdio.h>

extern const char* POKGAME_NAME;

/* include target-specific code */
#if defined(POKGAME_POSIX)
#include "error-posix.c"
#elif defined(POKGAME_WIN32)

#endif

/* target-independent code */

void pok_error(enum pok_errorkind kind,const char* message)
{
    if (kind == pok_error_warning)
        fprintf(stderr,"%s: warning: %s\n",POKGAME_NAME,message);
    else if (kind == pok_error_unimplemented)
        fprintf(stderr,"%s: unimplemented: %s",POKGAME_NAME,message);
    else if (kind == pok_error_fatal) {
        fprintf(stderr,"%s: fatal error: %s\n",POKGAME_NAME,message);
        exit(EXIT_FAILURE);
    }
}

/* pok exception handling; record exceptions on a per-thread basis */

static struct hashmap exceptions;
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

void pok_exception_load()
{
    hashmap_init(&exceptions,25,(hash_function)hash_int,(key_comparator)intcmp);
}
void pok_exception_unload()
{
    hashmap_delete_ex(&exceptions,(destructor)thread_exception_item_dstor);
}
struct pok_exception* pok_exception_new()
{
    int tid;
    struct thread_exception_item* list;
    struct pok_exception* ex = malloc(sizeof(struct pok_exception));
    ex->exID = -1;
    ex->category = pok_ex_default;
    ex->message = NULL;
    /* place exceptions in data structure */
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
    return ex;
}
struct pok_exception* pok_exception_new_ex(enum pok_ex_kind kind,int id)
{
    int tid;
    struct thread_exception_item* list;
    struct pok_exception* ex = malloc(sizeof(struct pok_exception));
    ex->exID = id;
    ex->category = kind;
    ex->message = NULL;
    /* place exceptions in data structure */
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list == NULL) {
        list = malloc(sizeof(struct thread_exception_item));
        list->id = tid;
        stack_init(&list->exceptions);
        hashmap_insert(&exceptions,list);
    }
    stack_push(&list->exceptions,ex);
    return ex;
}
bool_t pok_exception_check()
{
    int tid;
    struct thread_exception_item* list;
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list==NULL || stack_is_empty(&list->exceptions))
        return FALSE;
    return TRUE;
}
bool_t pok_exception_check_ex(enum pok_ex_kind kind)
{
    int i;
    int tid;
    void** buf;
    struct thread_exception_item* list;
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list!=NULL && !stack_is_empty(&list->exceptions))
        for (i = 0,buf = stack_getbuffer(&list->exceptions);i < stack_getsize(&list->exceptions);++i)
            if (((struct pok_exception*)buf[i])->category == kind)
                return TRUE;
    return FALSE;
}
const struct pok_exception* pok_exception_pop()
{
    int tid;
    struct pok_exception* ex;
    struct thread_exception_item* list;
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list==NULL || stack_is_empty(&list->exceptions))
        return NULL;
    ex = stack_pop(&list->exceptions);
    if (list->last != NULL)
        free(list->last);
    list->last = ex;
    return ex;
}
const struct pok_exception* pok_exception_pop_ex(enum pok_ex_kind kind)
{
    int tid;
    struct pok_exception* ex;
    struct thread_exception_item* list;
    tid = pok_get_thread_id();
    list = hashmap_lookup(&exceptions,&tid);
    if (list==NULL || stack_is_empty(&list->exceptions) || 
        ((struct pok_exception*)stack_top(&list->exceptions))->category!=kind)
        return NULL;
    ex = stack_pop(&list->exceptions);
    if (list->last != NULL)
        free(list->last);
    list->last = ex;
    return ex;
}
