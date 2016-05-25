/* netobj.h */
#ifndef POKGAME_NETOBJ_H
#define POKGAME_NETOBJ_H
#include "net.h"
#include "protocol.h"

/* constants */
enum pok_netobj_constants
{
    UNUSED_NETOBJ_ID = 0
};

/* exceptions */
enum pok_ex_netobj
{
    pok_ex_netobj_bad_id
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

/* enumerate the static network object kinds (graphics subsystem is an exception) */
enum pok_static_obj_kind
{
    pok_static_obj_tile_manager,
    pok_static_obj_sprite_manager,
    _pok_static_obj_top /* this enumerator should represent the total number of static network object kinds */
};

/* enumerate the dynamic network object kinds */
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
struct pok_netobj_upinfo; /* used with 'netmethod_send' */

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
bool_t pok_netobj_register(struct pok_netobj* netobj,uint32_t id);
enum pok_network_result pok_netobj_netread(struct pok_netobj* netobj,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info);
enum pok_network_result pok_netobj_netwrite(struct pok_netobj* netobj,struct pok_data_source* dsrc,struct pok_netobj_writeinfo* info);
#define POK_NETOBJ(obj) ((struct pok_netobj*)obj)

/* define generic typedefs for netobj functions (e.g. netread/netwrite) */
typedef enum pok_network_result (*netread_func_t)(struct pok_netobj*,
    struct pok_data_source*,struct pok_netobj_readinfo*);
typedef enum pok_network_result (*netwrite_func_t)(struct pok_netobj*,
    struct pok_data_source*,struct pok_netobj_writeinfo*);

/* network object functionality: we need to track changes to dynamic network objects, so they are
   stored in a database by their unique id numbers; the database is global and not thread-safe */
void pok_netobj_load_module();
void pok_netobj_unload_module();
uint32_t pok_netobj_allocate_unique_id();
struct pok_netobj* pok_netobj_lookup(uint32_t id);

/* pok_netobj_readinfo: used by the implementation when reading a network operation (either
   a 'netread' or 'netmethod_recv'; it may be used in any way the implementation sees fit */
struct pok_netobj_readinfo
{
    uint16_t fieldCnt; /* field counter */
    uint16_t fieldProg; /* field progress */
    uint16_t depth[2]; /* progress within current field; 2 variables allow multiple dimensions to the field */

    /* form a linked list to represent netobj info for potential substructures */
    struct pok_netobj_readinfo* next;

    /* auxilary data object; will be freed automatically if set */
    void* aux;

    /* if non-zero then an incomplete transfer sent at least some bytes but not all */
    bool_t pending;
};
struct pok_netobj_readinfo* pok_netobj_readinfo_new();
void pok_netobj_readinfo_free(struct pok_netobj_readinfo* info);
void pok_netobj_readinfo_init(struct pok_netobj_readinfo* info);
void pok_netobj_readinfo_delete(struct pok_netobj_readinfo* info);
void pok_netobj_readinfo_reset(struct pok_netobj_readinfo* info);
enum pok_network_result pok_netobj_readinfo_process(struct pok_netobj_readinfo* info);
enum pok_network_result pok_netobj_readinfo_process_depth(struct pok_netobj_readinfo* info,int index);
bool_t pok_netobj_readinfo_alloc_next(struct pok_netobj_readinfo* info);

/* pok_netobj_writeinfo: used by the implementation when sending a network object (either
   in full or an update); the implementation uses the structure to remember how much information
   it has sent (the progress of the operation) and can use it in any way it sees fit */
struct pok_netobj_writeinfo
{
    uint16_t fieldCnt;  /* field counter */
    uint16_t fieldProg; /* field progress counter */
    uint16_t depth[2];  /* multidimensional depth counter */

    /* if non-zero then an incomplete transfer sent at least some bytes but not all */
    bool_t pending;
};
struct pok_netobj_writeinfo* pok_netobj_writeinfo_new();
void pok_netobj_writeinfo_free(struct pok_netobj_writeinfo* info);
void pok_netobj_writeinfo_init(struct pok_netobj_writeinfo* info);
enum pok_network_result pok_netobj_writeinfo_process(struct pok_netobj_writeinfo* info);
enum pok_network_result pok_netobj_writeinfo_process_depth(struct pok_netobj_writeinfo* info,int index);

struct pok_netobj_upinfo
{
    int32_t methodID;
    union {
        /* this union aggregates the method parameter unions defined in 'protocol.h' */
        union pok_map_chunk_method_params mapChunk;
        union pok_map_method_params map;
    } methodParams;
};
void pok_netobj_upinfo_init(struct pok_netobj_upinfo* info,int32_t methodID);

#endif
