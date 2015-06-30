/* netobj.c - pokgame */
#include "netobj.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <dstructs/treemap.h>

/* pok_netobj module functionality */
static uint32_t idtop = 1; /* 1 is the first valid network id */
static struct treemap netobjs; /* this treemap stores references to network objects that we have received from our peer */
static inline int network_object_compar(struct pok_netobj* left,struct pok_netobj* right)
{
    return left->id - right->id;
}
void pok_netobj_load_module()
{
    treemap_init(&netobjs,(key_comparator)network_object_compar,NULL);
}
void pok_netobj_unload_module()
{
    treemap_delete(&netobjs);
}
uint32_t pok_netobj_allocate_unique_id()
{
    /* we never recycle ids; just get the next available one */
    return idtop++;
}
struct pok_netobj* pok_netobj_lookup(uint32_t id)
{
    /* create a dummy object for comparison */
    struct pok_netobj dummy;
    dummy.id = id;
    return treemap_lookup(&netobjs,&dummy);
}

/* netobj */
void pok_netobj_default(struct pok_netobj* netobj)
{
    netobj->id = UNUSED_NETOBJ_ID;
    netobj->kind = pok_netobj_unknown;
}
void pok_netobj_default_ex(struct pok_netobj* netobj,enum pok_netobj_kind kind)
{
    netobj->id = UNUSED_NETOBJ_ID;
    netobj->kind = kind;
}
void pok_netobj_delete(struct pok_netobj* netobj)
{
    if (netobj->id != UNUSED_NETOBJ_ID)
        treemap_remove(&netobjs,netobj);
}
bool_t pok_netobj_register(struct pok_netobj* netobj,uint32_t id)
{
    /* give the network object the specified id and register it in the database */
#ifdef POKGAME_DEBUG
    /* assert that the object is not assigned a network id */
    if (netobj->id == UNUSED_NETOBJ_ID)
        pok_error(pok_error_fatal,"pok_netobj_register(): network object already assigned id");
#endif

    netobj->id = id;
    if (treemap_insert(&netobjs,netobj) != 0) {
        netobj->id = UNUSED_NETOBJ_ID;
        pok_exception_new_ex(pok_ex_netobj,pok_ex_netobj_bad_id);
        return FALSE;
    }
    return TRUE;
}
enum pok_network_result pok_netobj_netread(struct pok_netobj* netobj,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    /* fields
       [4 bytes] id
     */
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint32(dsrc,&netobj->id);
        result = pok_netobj_readinfo_process(info);
        /* if successful, add the network object to the system */
        if (result == pok_net_completed && treemap_insert(&netobjs,netobj) != 0) {
            /* id should be unique; also, we should only 'netread' an object
               once; this means the peer forgot that they already sent this object */
            pok_exception_new_ex(pok_ex_netobj,pok_ex_netobj_bad_id);
            return pok_net_failed_protocol;
        }
    }
    return result;
}

/* pok_netobj_readinfo */
struct pok_netobj_readinfo* pok_netobj_readinfo_new()
{
    struct pok_netobj_readinfo* info;
    info = malloc(sizeof(struct pok_netobj_readinfo));
    if (info == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_netobj_readinfo_init(info);
    return info;
}
void pok_netobj_readinfo_free(struct pok_netobj_readinfo* info)
{
    pok_netobj_readinfo_delete(info);
    free(info);
}
void pok_netobj_readinfo_init(struct pok_netobj_readinfo* info)
{
    info->fieldCnt = 0;
    info->fieldProg = 0;
    info->depth[0] = 0;
    info->depth[1] = 0;
    info->next = NULL;
    info->aux = NULL;
}
void pok_netobj_readinfo_reset(struct pok_netobj_readinfo* info)
{
    info->fieldCnt = 0;
    info->fieldProg = 0;
    info->depth[0] = 0;
    info->depth[1] = 0;
    if (info->aux != NULL) {
        free(info->aux);
        info->aux = NULL;
    }
}
void pok_netobj_readinfo_delete(struct pok_netobj_readinfo* info)
{
    if (info->next != NULL)
        pok_netobj_readinfo_free(info->next);
    if (info->aux != NULL)
        free(info->aux);
}
enum pok_network_result pok_netobj_readinfo_process(struct pok_netobj_readinfo* info)
{
    const struct pok_exception* ex;
    ex = pok_exception_peek();
    if (ex != NULL) {
        if (ex->kind==pok_ex_net && (ex->id==pok_ex_net_wouldblock || ex->id==pok_ex_net_pending)) {
            /* mark pending flag so user can know exactly why the transfer was incomplete */
            info->pending = ex->id == pok_ex_net_pending;
            /* remove exception since it was processed here */
            pok_exception_pop();
            return pok_net_incomplete;
        }
        /* leave the exception for the calling context */
        return pok_net_failed;
    }
    ++info->fieldProg;
    return pok_net_completed;
}
enum pok_network_result pok_netobj_readinfo_process_depth(struct pok_netobj_readinfo* info,int index)
{
    const struct pok_exception* ex;
    ex = pok_exception_peek();
    if (ex != NULL) {
        if (ex->kind==pok_ex_net && (ex->id==pok_ex_net_wouldblock || ex->id==pok_ex_net_pending)) {
            /* mark pending flag so user can know exactly why the transfer was incomplete */
            info->pending = ex->id == pok_ex_net_pending;
            /* remove exception since it was processed here */
            pok_exception_pop();
            return pok_net_incomplete;
        }
        /* leave the exception for the calling context */
        return pok_net_failed;
    }
    ++info->depth[index%2];
    return pok_net_completed;
}
bool_t pok_netobj_readinfo_alloc_next(struct pok_netobj_readinfo* info)
{
    if (info->next == NULL) {
        info->next = pok_netobj_readinfo_new();
        if (info->next == NULL)
            return FALSE;
    }
    else
        pok_netobj_readinfo_reset(info->next);
    return TRUE;
}

/* pok_netobj_writeinfo */
struct pok_netobj_writeinfo* pok_netobj_writeinfo_new()
{
    struct pok_netobj_writeinfo* info;
    info = malloc(sizeof(struct pok_netobj_writeinfo));
    if (info == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_netobj_writeinfo_init(info);
    return info;
}
void pok_netobj_writeinfo_free(struct pok_netobj_writeinfo* info)
{
    free(info);
}
void pok_netobj_writeinfo_init(struct pok_netobj_writeinfo* info)
{
    info->fieldCnt = 0;
    info->fieldProg = 0;
    info->depth[0] = info->depth[1] = 0;
}

/* pok_netobj_upinfo */
void pok_netobj_upinfo_init(struct pok_netobj_upinfo* info,int32_t methodID)
{
    memset(info,0,sizeof(struct pok_netobj_upinfo));
    info->methodID = methodID;
}
