/* net-posix.c - pokgame */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

struct pok_datasource
{
    /* mode:
       bits 0-1: cast to enum pok_iomode 
       bit 2: if 1, then device uses two descriptors */
    byte_t mode;
    
    /* file descriptors: if bit 2 of mode is 0, then only fd[0] is used */
    int fd[2];

    /* internal buffer to store read data for caller */
    size_t sz;
    byte_t buffer[4096];
};

struct pok_datasource* pok_datasource_new_local_named(const char* name)
{
    struct sockaddr_un addr;
    struct pok_datasource* ds = malloc(sizeof(struct pok_datasource));
    ds->fd[0] = socket(AF_UNIX,SOCK_STREAM,0);
    if (ds->fd[0] == -1)
        pok_error(pok_error_fatal,"cannot allocate socket resource");
    /* attempt connect to socket name */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path,name,sizeof(addr.sun_path));
    if (connect(ds->fd[0],(const struct sockaddr*)&addr,sizeof(struct sockaddr_un)) == -1) {

        free(ds);
        return NULL;
    }
    ds->sz = 0;
    ds->mode = pok_iomode_full_duplex;
    return ds;
}
struct pok_datasource* pok_datasource_new_local_anon()
{
    struct pok_datasource* ds = malloc(sizeof(struct pok_datasource));
    int r = pipe(ds->fd);
    ds->sz = 0;
    ds->mode = pok_iomode_full_duplex | (0x1 << 0x3);
    return ds;
}
struct pok_datasource* pok_datasource_new_network(struct pok_network_address* address)
{
    struct pok_datasource* ds = malloc(sizeof(struct pok_datasource));
    ds->fd[0] = socket(AF_INET,SOCK_STREAM,0);
    (void)(address);
    return NULL;
}
struct pok_datasource* pok_datasource_new_file(const char* filename,enum pok_filemode mode)
{
    (void)(filename);
    (void)(mode);
    return NULL;
}
