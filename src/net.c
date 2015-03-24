/* net.c - pokgame */
#include "net.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <dstructs/treemap.h>

static inline bool_t pok_data_source_readbuf_full(struct pok_data_source* dsrc);
static inline bool_t pok_data_source_endofcomms(struct pok_data_source* dsrc);

/* include target-specific code */
#if defined(POKGAME_POSIX)
#include "net-posix.c"
#elif defined(POKGAME_WIN32)

#endif

/* target-independent code */

/* pok_network_object_info */
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
}
enum pok_network_result pok_netobj_readinfo_process(struct pok_netobj_readinfo* info)
{
    const struct pok_exception* ex;
    ex = pok_exception_peek();
    if (ex != NULL) {
        if (ex->kind==pok_ex_net && (ex->id==pok_ex_net_wouldblock || ex->id==pok_ex_net_pending)) {
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

static struct treemap netobjs;
static inline int network_object_compar(struct pok_netobj* left,struct pok_netobj* right)
{
    return left->id - right->id;
}
void network_object_load()
{
    treemap_init(&netobjs,(key_comparator)network_object_compar,NULL);
}
void network_object_unload()
{
    treemap_delete(&netobjs);
}
struct pok_netobj* network_object_lookup(uint32_t id)
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
enum pok_network_result pok_netobj_netread(struct pok_netobj* netobj,struct pok_data_source* dsrc,struct pok_netobj_readinfo* info)
{
    enum pok_network_result result = pok_net_already;
    if (info->fieldProg == 0) {
        pok_data_stream_read_uint32(dsrc,&netobj->id);
        result = pok_netobj_readinfo_process(info);
        /* if successful, add the network object to the system */
        if (result == pok_net_completed) {
            treemap_insert(&netobjs,netobj);
            netobj->kind = pok_netobj_unknown;
        }
    }
    return result;
}

/* these functions convert between byte streams and binary integer
   variables; to ensure the correct endianness, bitwise operations
   are performed; little-endian is used unless a compiler flag is set */
static void to_bin16(const byte_t* src,uint16_t* dst)
{
    int i;
    *dst = 0;
#ifdef POKGAME_BIG_ENDIAN
    for (i = 1;i >= 0;--i)
#else
    for (i = 0;i < 2;++i)
#endif
        *dst |= *(src+i) << (8*i);
}
static void from_bin16(uint16_t src,byte_t* dst)
{
    int i, j = 0;
#ifdef POKGAME_BIG_ENDIAN
    for (i = 1;i >= 0;--i)
#else
    for (i = 0;i < 2;++i)
#endif
        dst[i] = (src>>(8*j++)) & 0xff;
}
static void to_bin32(const byte_t* src,uint32_t* dst)
{
    size_t i;
    *dst = 0;
#ifdef POKGAME_BIG_ENDIAN
    for (i = 3;i >= 0;--i)
#else
    for (i = 0;i < 4;++i)
#endif
        *dst |= *(src+i) << (8*i);
}
static void from_bin32(uint32_t src,byte_t* dst)
{
    int i, j = 0;
#ifdef POKGAME_BIG_ENDIAN
    for (i = 3;i >= 0;--i)
#else
    for (i = 0;i < 4;++i)
#endif
        dst[i] = (src>>(8*j++)) & 0xff;
}
static void to_bin64(const byte_t* src,uint64_t* dst)
{
    size_t i;
    *dst = 0;
#ifdef POKGAME_BIG_ENDIAN
    for (i = 7;i >= 0;--i)
#else
    for (i = 0;i < 8;++i)
#endif
        *dst |= *(src+i) << (8*i);
}
static void from_bin64(uint64_t src,byte_t* dst)
{
    int i, j = 0;
#ifdef POKGAME_BIG_ENDIAN
    for (i = 7;i >= 0;--i)
#else
    for (i = 0;i < 8;++i)
#endif
        dst[i] = (src>>(8*j++)) & 0xff;
}

bool_t pok_data_stream_fread(struct pok_data_source* dsrc,int* cnt,const char* format, ...)
{
    /* read values based on format string; supported characters are 'b', 'i', 'd', 'l', 's';
       return FALSE if an exception was generated; the number of correctly read elements is
       assigned to '*cnt' */
    va_list args;
    va_start(args,format);
    *cnt = 0;
    while (*format) {
        int c = *format++;
        byte_t* data;
        size_t bytesRead = 0, bytesNeeded = 0;
        if ( isspace(c) )
            continue;
        if (c == 'b') {
            bytesNeeded = 1;
            data = pok_data_source_read(dsrc,1,&bytesRead);
            if (data!=NULL && bytesRead==1)
                *va_arg(args,byte_t*) = data[0];
        }
        else if (c == 'i') {
            bytesNeeded = 2;
            data = pok_data_source_read(dsrc,2,&bytesRead);
            if (data!=NULL && bytesRead==2)
                to_bin16(data,va_arg(args,uint16_t*));
        }
        else if (c == 'd') {
            bytesNeeded = 4;
            data = pok_data_source_read(dsrc,4,&bytesRead);
            if (data!=NULL && bytesRead==4)
                to_bin32(data,va_arg(args,uint32_t*));
        }
        else if (c == 'l') {
            bytesNeeded = 8;
            data = pok_data_source_read(dsrc,8,&bytesRead);
            if (data!=NULL && bytesRead==8)
                to_bin64(data,va_arg(args,uint64_t*));
        }
        else if (c == 's') {
            /* read null terminated string */
            struct pok_string* dst = va_arg(args,struct pok_string*);
            pok_string_clear(dst);
            while (TRUE) {
                data = pok_data_source_read(dsrc,1,&bytesRead);
                if (data==NULL || bytesRead==0 || data[0]=='\0')
                    break;
                pok_string_concat_char(dst,data[0]);
            }
        }
        else
            pok_error(pok_error_fatal,"bad format character to pok_data_stream_fread()");
        if (data == NULL) {
            /* exception is inherited */
            va_end(args);
            return FALSE;
        }
        if (bytesRead == 0) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
            va_end(args);
            return FALSE;
        }
        if (bytesRead < bytesNeeded) {
            if ( pok_data_source_endofcomms(dsrc) ) {
                /* not enough data is available in the buffer and the end-of-comms
                   has been reached */
                pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
                va_end(args);
                return FALSE;
            }
            /* unread the bytes so the next iteration can grab them */
            pok_data_source_unread(dsrc,bytesRead);
        }
        else
            ++*cnt;
    }
    va_end(args);
    return TRUE;
}
bool_t pok_data_stream_read_byte(struct pok_data_source* dsrc,byte_t* dst)
{
    byte_t* data;
    size_t bytesRead;
    data = pok_data_source_read(dsrc,1,&bytesRead);
    if (data == NULL)
        /* exception is inherited */
        return FALSE;
    if (bytesRead == 0) {
        pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
        return FALSE;
    }
    *dst = data[0];
    return TRUE;
}
bool_t pok_data_stream_read_uint16(struct pok_data_source* dsrc,uint16_t* dst)
{
    byte_t* data;
    size_t bytesRead;
    data = pok_data_source_read(dsrc,2,&bytesRead);
    if (data == NULL)
        /* exception is inherited */
        return FALSE;
    if (bytesRead < 2) {
        if (bytesRead==0 || pok_data_source_endofcomms(dsrc))
            pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
        else {
            pok_data_source_unread(dsrc,bytesRead);
            pok_exception_new_ex(pok_ex_net,pok_ex_net_pending);
        }
        return FALSE;
    }
    to_bin16(data,dst);
    return TRUE;
}
bool_t pok_data_stream_read_uint32(struct pok_data_source* dsrc,uint32_t* dst)
{
    byte_t* data;
    size_t bytesRead;
    data = pok_data_source_read(dsrc,4,&bytesRead);
    if (data == NULL)
        /* exception is inherited */
        return FALSE;
    if (bytesRead < 4) {
        if (bytesRead==0 || pok_data_source_endofcomms(dsrc))
            pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
        else {
            pok_data_source_unread(dsrc,bytesRead);
            pok_exception_new_ex(pok_ex_net,pok_ex_net_pending);
        }
        return FALSE;
    }
    to_bin32(data,dst);
    return TRUE;
}
bool_t pok_data_stream_read_uint64(struct pok_data_source* dsrc,uint64_t* dst)
{
    byte_t* data;
    size_t bytesRead;
    data = pok_data_source_read(dsrc,8,&bytesRead);
    if (data == NULL)
        /* exception is inherited */
        return FALSE;
    if (bytesRead < 8) {
        if (bytesRead==0 || pok_data_source_endofcomms(dsrc))
            pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
        else {
            pok_data_source_unread(dsrc,bytesRead);
            pok_exception_new_ex(pok_ex_net,pok_ex_net_pending);
        }
        return FALSE;
    }
    to_bin64(data,dst);
    return TRUE;
}
bool_t pok_data_stream_read_string(struct pok_data_source* dsrc,char* dst,size_t numBytes)
{
    /* reads a string of the specified size; data might be pending; the string is null-terminated
       if successfully read */
    byte_t* data;
    size_t bytesIn;
    data = pok_data_source_read(dsrc,numBytes,&bytesIn);
    if (data == NULL)
        return FALSE;
    if (bytesIn == 0) {
        pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
        return FALSE;
    }
    if (bytesIn==numBytes || pok_data_source_readbuf_full(dsrc) || pok_data_source_endofcomms(dsrc)) {
        strncpy(dst,(const char*)data,bytesIn);
        dst[bytesIn] = 0;
        return TRUE;
    }
    /* flag that data is pending; user can call function later to retrieve buffered data */
    pok_data_source_unread(dsrc,bytesIn);
    pok_exception_new_ex(pok_ex_net,pok_ex_net_pending);
    return FALSE;
}
bool_t pok_data_stream_read_string_ex(struct pok_data_source* dsrc,struct pok_string* dst)
{
    /* reads a null-terminated string from the data source; if data is pending (no null byte
       has been received yet and the stream has not received the end of comms) then the function
       returns FALSE with a pending exception pushed on the exception stack; as a consequence,
       this function must solely concatenate the bytes, and the user must be responsible for
       clearing a non-empty string */
    static const size_t ATTEMPT_SIZE = 1024;
    size_t iter;
    byte_t* data;
    size_t bytesIn;
    data = pok_data_source_read(dsrc,ATTEMPT_SIZE,&bytesIn);
    if (data == NULL)
        /* exception is inherited from above function call */
        return FALSE;
    /* read until null-terminator */
    for (iter = 0;iter < bytesIn;++iter)
        if (data[iter] == 0)
            break;
    pok_string_concat_ex(dst,(const char*)dst,iter);
    if (iter < bytesIn) {
        pok_data_source_unread(dsrc,ATTEMPT_SIZE-iter);
        return TRUE;
    }
    if ( pok_data_source_endofcomms(dsrc) )
        pok_exception_new_ex(pok_ex_net,pok_ex_net_endofcomms);
    else
        /* don't unread the data; keep it in the string buffer; then the user should
           call back again until we get a null terminator */
        pok_exception_new_ex(pok_ex_net,pok_ex_net_pending);
    return FALSE;
}
bool_t pok_data_stream_fwrite(struct pok_data_source* dsrc,int* cnt,const char* format, ...)
{
    /* similar to pok_data_stream_fread but for writing */
    va_list args;
    va_start(args,format);
    *cnt = 0;
    while (*format) {
        bool_t err;
        int c = *format++;
        if ( isspace(c) )
            continue;
        if (c == 'b')
            /* note that byte_t is promoted to int */
            err = pok_data_stream_write_byte(dsrc,va_arg(args,int));
        else if (c == 'i')
            /* note that uint16_t is promoted to int */
            err = pok_data_stream_write_uint16(dsrc,va_arg(args,int));
        else if (c == 'd')
            err = pok_data_stream_write_uint32(dsrc,va_arg(args,uint32_t));
        else if (c == 'l')
            err = pok_data_stream_write_uint64(dsrc,va_arg(args,uint64_t));
        else if (c == 's')
            err = pok_data_stream_write_string(dsrc,va_arg(args,const char*));
        else
            pok_error(pok_error_fatal,"bad format character to pok_data_stream_fwrite()");
        if (!err) {
            /* inherit exception */
            va_end(args);
            return FALSE;
        }
        ++*cnt;
    }
    va_end(args);
    return TRUE;
}
bool_t pok_data_stream_write_byte(struct pok_data_source* dsrc,byte_t src)
{
    size_t bytesOut;
    if ( pok_data_source_write(dsrc,&src,1,&bytesOut) ) {
        if (bytesOut < 1)
            return pok_data_source_save(dsrc,&src,1);
        return TRUE;
    }
    if (bytesOut > 0) {
        pok_exception_pop();
        return TRUE;
    }
    return FALSE;
}
bool_t pok_data_stream_write_uint16(struct pok_data_source* dsrc,uint16_t src)
{
    size_t bytesOut;
    byte_t data[2];
    from_bin16(src,data);
    if ( pok_data_source_write(dsrc,data,2,&bytesOut) ) {
        if (bytesOut<2 && !pok_data_source_save(dsrc,data+bytesOut,2-bytesOut)) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_noroom);
            return FALSE;
        }
        return TRUE;
    }
    /* inherit exception */
    return FALSE;
}
bool_t pok_data_stream_write_uint32(struct pok_data_source* dsrc,uint32_t src)
{
    size_t bytesOut;
    byte_t data[4];
    from_bin32(src,data);
    if ( pok_data_source_write(dsrc,data,4,&bytesOut) ) {
        if (bytesOut<4 && !pok_data_source_save(dsrc,data+bytesOut,4-bytesOut)) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_noroom);
            return FALSE;
        }
        return TRUE;
    }
    /* inherit exception */
    return FALSE;
}
bool_t pok_data_stream_write_uint64(struct pok_data_source* dsrc,uint64_t src)
{
    size_t bytesOut;
    byte_t data[8];
    from_bin64(src,data);
    if ( pok_data_source_write(dsrc,data,8,&bytesOut) ) {
        if (bytesOut<8 && !pok_data_source_save(dsrc,data+bytesOut,8-bytesOut)) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_noroom);
            return FALSE;
        }
        return TRUE;
    }
    /* inherit exception */
    return FALSE;
}
bool_t pok_data_stream_write_string(struct pok_data_source* dsrc,const char* src)
{
    size_t len, bytesOut;
    len = strlen(src);
    if ( pok_data_source_write(dsrc,(byte_t*)src,len,&bytesOut) ) {
        if (bytesOut<len && !pok_data_source_save(dsrc,(const byte_t*)src+bytesOut,len-bytesOut)) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_noroom);
            return FALSE;
        }
        return TRUE;
    }
    /* inherit exception */
    return FALSE;
}
bool_t pok_data_stream_write_string_ex(struct pok_data_source* dsrc,const char* src,size_t numBytes)
{
    size_t bytesOut;
    if ( pok_data_source_write(dsrc,(byte_t*)src,numBytes,&bytesOut) ) {
        if (bytesOut<numBytes && !pok_data_source_save(dsrc,(const byte_t*)src+bytesOut,numBytes-bytesOut)) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_noroom);
            return FALSE;
        }
        return TRUE;
    }
    /* inherit exception */
    return FALSE;
}
bool_t pok_data_stream_write_string_obj(struct pok_data_source* dsrc,const struct pok_string* src)
{
    size_t bytesOut;
    if ( pok_data_source_write(dsrc,(byte_t*)src->buf,src->len,&bytesOut) ) {
        if (bytesOut<src->len && !pok_data_source_save(dsrc,(const byte_t*)src->buf+bytesOut,src->len-bytesOut)) {
            pok_exception_new_ex(pok_ex_net,pok_ex_net_noroom);
            return FALSE;
        }
        return TRUE;
    }
    /* inherit exception */
    return FALSE;
}
