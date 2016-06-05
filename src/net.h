/* net.h - pokgame */
#ifndef POKGAME_NET_H
#define POKGAME_NET_H
#include "types.h"

/* exception ids generated by this module */
enum pok_ex_net
{
    pok_ex_net_unspec, /* unspecified error */
    pok_ex_net_interrupt, /* read/write interruption */
    pok_ex_net_wouldblock, /* asynchronous call returned/sent no data */
    pok_ex_net_pending, /* call did not return/send enough data (some data was still received) */
    pok_ex_net_brokenpipe, /* write to unconnected pipe */
    pok_ex_net_endofcomms, /* end of communication sent on IO device */
    pok_ex_net_noroom, /* the data source attempted to buffer bytes left over from a write operation but there was no room */

    /* these are flagged when 'pok_data_source_new_file' fails */
    pok_ex_net_file_does_not_exist,
    pok_ex_net_file_already_exist,
    pok_ex_net_file_permission_denied,
    pok_ex_net_file_bad_path,

    /* these are flagged when 'pok_data_source_new' OR 'pok_process_new' fail */
    pok_ex_net_could_not_create_local,
    pok_ex_net_could_not_create_named_local,
    pok_ex_net_could_not_create_remote,
    pok_ex_net_could_not_create_process,
    pok_ex_net_bad_program,
    pok_ex_net_program_not_found,
    pok_ex_net_execute_denied
};

/* IPv4 network address information */
struct pok_network_address
{
    byte_t ipv4[4];
    uint16_t port;
};
void pok_network_address_init(struct pok_network_address* addr,const char* host,const char* port);

/* filemodes for file 'datasource' */
enum pok_filemode
{
    pok_filemode_create_new, /* file must be new */
    pok_filemode_create_always, /* create new file; file can preexist but is truncated and overwritten */
    pok_filemode_open_existing /* file must preexist */
};

/* bitmask input-output modes for any 'datasource' */
enum pok_iomode
{
    pok_iomode_read = 0x01,
    pok_iomode_write = 0x02,
    pok_iomode_full_duplex = 0x03
};

/* pok_data_source: an abstraction to a lower-level input-output device supported by the 
   operating system; to the rest of the application it is an opaque type used to receive
   and send data to another process, either local or remote */
struct pok_data_source;
struct pok_data_source* pok_data_source_new_standard();
struct pok_data_source* pok_data_source_new_local_named(const char* name);
struct pok_data_source* pok_data_source_new_local_anon();
struct pok_data_source* pok_data_source_new_network(struct pok_network_address* address);
struct pok_data_source* pok_data_source_new_file(const char* filename,enum pok_filemode mode,enum pok_iomode access);
byte_t* pok_data_source_read(struct pok_data_source* dsrc,size_t bytesRequested,size_t* bytesRead);
byte_t* pok_data_source_read_any(struct pok_data_source* dsrc,size_t maxBytes,size_t* bytesRead);
bool_t pok_data_source_read_to_buffer(struct pok_data_source* dsrc,void* buffer,size_t bytesRequested,size_t* bytesRead);
char pok_data_source_peek(struct pok_data_source* dsrc); /* these return (char) -1 on failure so use them for plain text */
char pok_data_source_peek_ex(struct pok_data_source* dsrc,size_t lookahead);
char pok_data_source_pop(struct pok_data_source* dsrc);
bool_t pok_data_source_write(struct pok_data_source* dsrc,const byte_t* buffer,size_t size,size_t* bytesWritten);
void pok_data_source_buffering(struct pok_data_source* dsrc,bool_t on);
void pok_data_source_unread(struct pok_data_source* dsrc,size_t size);
bool_t pok_data_source_save(struct pok_data_source* dsrc,const byte_t* buffer,size_t size);
bool_t pok_data_source_flush(struct pok_data_source* dsrc);
enum pok_iomode pok_data_source_getmode(struct pok_data_source* dsrc);
void pok_data_source_free(struct pok_data_source* dsrc);

/* higher-level data-stream operations */
bool_t pok_data_stream_fread(struct pok_data_source* dsrc,int* cnt,const char* format, ...);
bool_t pok_data_stream_read_byte(struct pok_data_source* dsrc,byte_t* dst);
bool_t pok_data_stream_read_uint16(struct pok_data_source* dsrc,uint16_t* dst);
static inline bool_t pok_data_stream_read_int16(struct pok_data_source* dsrc,int16_t* dst)
{ return pok_data_stream_read_uint16(dsrc,(uint16_t*)dst); }
bool_t pok_data_stream_read_uint32(struct pok_data_source* dsrc,uint32_t* dst);
static inline bool_t pok_data_stream_read_int32(struct pok_data_source* dsrc,int32_t* dst)
{ return pok_data_stream_read_uint32(dsrc,(uint32_t*)dst); }
bool_t pok_data_stream_read_uint64(struct pok_data_source* dsrc,uint64_t* dst);
static inline bool_t pok_data_stream_read_int64(struct pok_data_source* dsrc,int64_t* dst)
{ return pok_data_stream_read_uint64(dsrc,(uint64_t*)dst); }
bool_t pok_data_stream_read_string(struct pok_data_source* dsrc,char* dst,size_t numBytes);
bool_t pok_data_stream_read_string_ex(struct pok_data_source* dsrc,struct pok_string* dst);
bool_t pok_data_stream_fwrite(struct pok_data_source* dsrc,int* cnt,const char* format, ...);
bool_t pok_data_stream_write_byte(struct pok_data_source* dsrc,byte_t src);
bool_t pok_data_stream_write_uint16(struct pok_data_source* dsrc,uint16_t src);
static inline bool_t pok_data_stream_write_int16(struct pok_data_source* dsrc,int16_t src)
{ return pok_data_stream_write_uint16(dsrc,src); }
bool_t pok_data_stream_write_uint32(struct pok_data_source* dsrc,uint32_t src);
static inline bool_t pok_data_stream_write_int32(struct pok_data_source* dsrc,int32_t src)
{ return pok_data_stream_write_uint32(dsrc,src); }
bool_t pok_data_stream_write_uint64(struct pok_data_source* dsrc,uint64_t src);
static inline bool_t pok_data_stream_write_int64(struct pok_data_source* dsrc,int64_t src)
{ return pok_data_stream_write_uint64(dsrc,src); }
bool_t pok_data_stream_write_string(struct pok_data_source* dsrc,const char* src);
bool_t pok_data_stream_write_string_ex(struct pok_data_source* dsrc,const char* src,size_t numBytes);
bool_t pok_data_stream_write_string_obj(struct pok_data_source* dsrc,const struct pok_string* src);

/* pok_process: an abstraction around starting a process; its implementation is platform specific */
#define PROCESS_TIMEOUT_INFINITE -1
enum pok_process_state
{
    pok_process_state_running,   /* the process is still running */
    pok_process_state_killed,    /* the process terminated abnormally */
    pok_process_state_terminated /* the process terminated normally */
};

struct pok_process;
struct pok_process* pok_process_new(const char* cmdline,const char* environment,pok_error_callback errorCallback);
void pok_process_free(struct pok_process* proc);
enum pok_process_state pok_process_shutdown(struct pok_process* proc,int timeout,struct pok_data_source* procstdio);
struct pok_data_source* pok_process_stdio(struct pok_process* proc);
bool_t pok_process_has_terminated(struct pok_process* proc);

/* pok_thread: an abstraction around starting a new thread; its implementation is platform specific */
struct pok_thread;
typedef int (*pok_thread_entry)(void* param);
struct pok_thread* pok_thread_new(pok_thread_entry entryPoint,void* parameter);
void pok_thread_free(struct pok_thread* thread);
void pok_thread_start(struct pok_thread* thread);
int pok_thread_join(struct pok_thread* thread);

#endif
