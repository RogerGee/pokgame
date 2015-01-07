/* net.h - pokgame */
#ifndef POKGAME_NET_H
#define POKGAME_NET_H
#include "types.h"

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
    pok_filemode_create_always, /* file can preexist but is truncated and overwritten */
    pok_filemode_open_existing /* file must preexist */
};

/* bitmask input-output modes for any 'datasource' */
enum pok_iomode
{
    pok_iomode_read = 0x01,
    pok_iomode_write = 0x02,
    pok_iomode_full_duplex = 0x03
};

/* a 'datasource' is an abstraction to a lower-level input-output device supported by the 
   operating system; to the rest of the application it is an opaque type used to receive
   and send data to another process, either local or remote;  */
struct pok_datasource;

struct pok_datasource* pok_datasource_new_local_named(const char* name);
struct pok_datasource* pok_datasource_new_local_anon();
struct pok_datasource* pok_datasource_new_network(struct pok_network_address* address);
struct pok_datasource* pok_datasource_new_file(const char* filename,enum pok_filemode mode);
byte_t* pok_datasource_read(struct pok_datasource* ds,size_t* bytesRead);
size_t pok_datasource_write(struct pok_datasource* ds,const byte_t* buffer,size_t size);
enum pok_iomode pok_datasource_getmode(struct pok_datasource* ds);
bool_t pok_datasource_shutdown(struct pok_datasource* ds,enum pok_iomode terminateMode);
void pok_datasource_destroy(struct pok_datasource* ds);

#endif
