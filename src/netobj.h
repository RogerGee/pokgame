/* netobj.h */
#ifndef POKGAME_NETOBJ_H
#define POKGAME_NETOBJ_H
#include "net.h"

/* constants */
enum pok_netobj_constants
{
    UNUSED_NETOBJ_ID = 0
};

/* flags for the sending/receiving of a network object */
enum pok_network_result
{
    pok_net_completed, /* object was successfully sent/received */
    pok_net_incomplete, /* object was not completely sent/received */
    pok_net_failed, /* object failed to send/be received; exception is generated */
    pok_net_failed_protocol, /* peer did not adhere to protocol; exception is generated */
    pok_net_failed_internal, /* object failed to send/be received but due to another reason; exception is generated */
    pok_net_already /* object was marked as already sent */
};

/* enumerate the network object kinds */
enum pok_netobj_kind
{
    pok_netobj_unknown,
    pok_netobj_world,
    pok_netobj_map,
    pok_netobj_mapchunk,
    pok_netobj_character
};

/* the following structures (defined later) are used to perform netobj methods */
struct pok_netobj_readinfo; /* used with 'netread' */
struct pok_netobj_writeinfo; /* used with 'netwrite' */
struct pok_netobj_upinfo; /* used with 'netupread' and 'netupwrite' */

/* represents the superclass for all dynamic network objects; this object
   cannot be updated, only initially sent/received */
struct pok_netobj
{
    uint32_t id; /* if 0 then id is not used (object is not being tracked) */
    enum pok_netobj_kind kind;
};
void pok_netobj_default(struct pok_netobj* netobj);
void pok_netobj_default_ex(struct pok_netobj* netobj,enum pok_netobj_kind kind);
void pok_netobj_delete(struct pok_netobj* netobj);
enum pok_network_result pok_netobj_netread(struct pok_netobj* netobj,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info);
enum pok_network_result pok_netobj_netwrite(struct pok_netobj* netobj,struct pok_data_source* dsrc,struct pok_netobj_writeinfo* info);

/* network object functionality: we need to track changes to dynamic network objects */
void pok_netobj_load_module();
void pok_netobj_unload_module();
uint32_t pok_netobj_allocate_unique_id();
struct pok_netobj* pok_netobj_lookup(uint32_t id);

/* network object information structure; used by the implementation
   to perform a 'netread' operation; it may be used in any way the
   implementation sees fit */
struct pok_netobj_readinfo
{
    uint16_t fieldCnt; /* field counter */
    uint16_t fieldProg; /* field progress */
    uint16_t depth[2]; /* progress within current field; 2 variables allow multiple dimensions to the field */

    /* form a linked list to represent netobj info for potential substructures */
    struct pok_netobj_readinfo* next;

    /* auxilary data object; will be freed automatically if set */
    void* aux;
};
struct pok_netobj_readinfo* pok_netobj_readinfo_new();
void pok_netobj_readinfo_free(struct pok_netobj_readinfo* info);
void pok_netobj_readinfo_init(struct pok_netobj_readinfo* info);
void pok_netobj_readinfo_delete(struct pok_netobj_readinfo* info);
void pok_netobj_readinfo_reset(struct pok_netobj_readinfo* info);
enum pok_network_result pok_netobj_readinfo_process(struct pok_netobj_readinfo* info);
enum pok_network_result pok_netobj_readinfo_process_depth(struct pok_netobj_readinfo* info,int index);
bool_t pok_netobj_readinfo_alloc_next(struct pok_netobj_readinfo* info);

#endif
