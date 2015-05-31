/* net-posix.c - pokgame */
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

/* pok_network_address */


/* pok_data_source */
enum pok_data_source_mode_flag
{
    DS_MODE_DEFAULT = 0x0,
    /* bits 0 and 1 are reserved */
    DS_MODE_FD_BOTH = 0x1 << 0x2,
    DS_MODE_BUFFER_OUTPUT = 0x1 << 0x3,
    DS_MODE_IS_SOCKET = 0x1 << 0x4,
    DS_MODE_REACH_EOF = 0x1 << 0x7
};

struct pok_data_source
{
    /* mode:
       bits 0-1: cast to enum pok_iomode 
       bit 2: if 1, then device uses two descriptors
       bit 3: if 1, then write output is buffered until flush or buffer full
       bit 4: if 1, then fd[0] is a socket descriptor

       bit 7: if 1, then fd[0] has reached EOF */
    byte_t mode;
    
    /* file descriptors: if bit 2 of mode is 0, then only fd[0] is used */
    int fd[2];

    /* internal buffer to store read data for caller */
    size_t szRead, itRead;
    byte_t bufferRead[4096];

    /* internal buffer to store write data for caller */
    size_t szWrite, itWrite;
    byte_t bufferWrite[4096];
};

static void pok_data_source_init(struct pok_data_source* dsrc,enum pok_iomode iomode)
{
    /* turn on output buffering by default */
    dsrc->mode |= (byte_t)(iomode & 0x3) | DS_MODE_BUFFER_OUTPUT;
    dsrc->szRead = 0;
    dsrc->itRead = 0;
    dsrc->szWrite = 0;
    dsrc->itWrite = 0;
}
struct pok_data_source* pok_data_source_new_local_named(const char* name)
{
    struct sockaddr_un addr;
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    dsrc->fd[0] = socket(AF_UNIX,SOCK_STREAM,0);
    if (dsrc->fd[0] == -1)
        pok_error(pok_error_fatal,"cannot create socket resource");
    /* attempt connect to socket name */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path,name,sizeof(addr.sun_path));
    if (connect(dsrc->fd[0],(const struct sockaddr*)&addr,sizeof(struct sockaddr_un)) == -1) {

        free(dsrc);
        return NULL;
    }
    dsrc->mode = DS_MODE_DEFAULT;
    pok_data_source_init(dsrc,pok_iomode_full_duplex);
    return dsrc;
}
struct pok_data_source* pok_data_source_new_local_anon()
{
    int r;
    struct pok_data_source* dsrc;
    dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    r = pipe(dsrc->fd);
    if (r == -1)
        pok_error(pok_error_fatal,"cannot create pipe resource");
    dsrc->mode = DS_MODE_FD_BOTH;
    pok_data_source_init(dsrc,pok_iomode_full_duplex);
    return dsrc;
}
struct pok_data_source* pok_data_source_new_network(struct pok_network_address* address)
{
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    dsrc->fd[0] = socket(AF_INET,SOCK_STREAM,0);
    (void)(address);
    dsrc->mode = DS_MODE_DEFAULT;
    pok_data_source_init(dsrc,pok_iomode_full_duplex);
    return dsrc;
}
struct pok_data_source* pok_data_source_new_file(const char* filename,enum pok_filemode mode,enum pok_iomode access)
{
    int flags; mode_t perms;
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    flags = 0;
    perms = 0666;
    if (mode == pok_filemode_create_new)
        flags |= O_CREAT | O_EXCL;
    else if (mode == pok_filemode_create_always)
        flags |= O_CREAT | O_TRUNC;
    if (access == pok_iomode_read)
        flags |= O_RDONLY;
    else if (access == pok_iomode_write)
        flags |= O_WRONLY;
    else if (access == pok_iomode_full_duplex)
        flags |= O_RDWR;
    dsrc->fd[0] = open(filename,flags,perms);
    if (dsrc->fd[0] == -1) {
        if (errno == EACCES)
            pok_exception_new_ex(pok_ex_net,pok_ex_net_file_permission_denied);
        else if (errno == EEXIST)
            pok_exception_new_ex(pok_ex_net,pok_ex_net_file_already_exist);
        else if (errno == EINTR)
            pok_exception_new_ex(pok_ex_net,pok_ex_net_interrupt);
        else if (errno==ENAMETOOLONG || errno==ENOTDIR || errno==EISDIR)
            pok_exception_new_ex(pok_ex_net,pok_ex_net_file_bad_path);
        else if (errno == ENOENT)
            pok_exception_new_ex(pok_ex_net,pok_ex_net_file_does_not_exist);
        else
            pok_exception_new_ex(pok_ex_default,pok_ex_default_undocumented);
        free(dsrc);
        return NULL;
    }
    dsrc->mode = DS_MODE_DEFAULT;
    pok_data_source_init(dsrc,access);
    return dsrc;
}
byte_t* pok_data_source_read(struct pok_data_source* dsrc,size_t bytesRequested,size_t* bytesRead)
{
    /* this function attempts to provide the user with a data buffer of the requested length read
       from the data source; it will return what it was able to read; NULL is returned if an
       exception was generated; if non-NULL is returned and *bytesRead==0 then the EOF was reached on
       the device */
    size_t it, remain;
    struct pok_exception* ex;
    /* check end of file on fd[0] */
    if (dsrc->mode & DS_MODE_REACH_EOF) {
        *bytesRead = 0;
        return dsrc->bufferRead;
    }
    /* cap max bytes able to read at buffer size */
    if (bytesRequested > sizeof(dsrc->bufferRead))
        bytesRequested = sizeof(dsrc->bufferRead);
    /* try read the remaining capacity of the buffer if we don't already
       have the requested number of bytes */
    if (bytesRequested > dsrc->szRead) {
        ssize_t r;
        /* compute remaining bytes */
        remain = sizeof(dsrc->bufferRead) - dsrc->szRead - dsrc->itRead;
        /* reduce used buffer portion to maximize capacity if needed */
        if (dsrc->itRead>0 && bytesRequested>remain) {
            remain += dsrc->itRead;
            if (dsrc->szRead > 0)
                memcpy(dsrc->bufferRead,dsrc->bufferRead+dsrc->itRead,dsrc->szRead);
            dsrc->itRead = 0;
        }
        if (remain > 0) {
            /* read from the device into the remaining buffer space */
            r = read(dsrc->fd[0],dsrc->bufferRead + dsrc->szRead + dsrc->itRead,remain);
            if (r == -1) {
                /* read error */
                ex = pok_exception_new();
                ex->kind = pok_ex_net;
                if (errno==EAGAIN || errno==EWOULDBLOCK)
                    ex->id = pok_ex_net_wouldblock;
                else if (errno == EINTR)
                    ex->id = pok_ex_net_interrupt;
                else
                    ex->id = pok_ex_net_unspec;
                *bytesRead = 0;
                return NULL;
            }
            if (r == 0)
                /* set EOF mode bit */
                dsrc->mode |= DS_MODE_REACH_EOF;
            dsrc->szRead += r;
        }
    }
    /* return a pointer to the specified memory */
    *bytesRead = dsrc->szRead > bytesRequested ? bytesRequested : dsrc->szRead;
    it = dsrc->itRead;
    dsrc->itRead += *bytesRead;
    dsrc->szRead -= *bytesRead;
    return dsrc->bufferRead + it;
}
bool_t pok_data_source_read_to_buffer(struct pok_data_source* dsrc,void* buffer,size_t bytesRequested,size_t* bytesRead)
{
    /* perform a simpler read into a user-provided buffer */
    ssize_t r, amt;
    if (dsrc->mode & DS_MODE_REACH_EOF) {
        *bytesRead = 0;
        return TRUE;
    }
    /* transfer bytes in our input buffer first; then read the remainder in directly to the user-provided buffer */
    if (dsrc->szRead > 0) {
        amt = bytesRequested > dsrc->szRead ? dsrc->szRead : bytesRequested;
        memcpy(buffer,dsrc->bufferRead + dsrc->itRead,amt);
        buffer = (char*)buffer + amt;
        bytesRequested -= amt;
        dsrc->szRead -= amt;
        dsrc->itRead += amt;
        *bytesRead = amt;
    }
    else
        *bytesRead = 0;
    if (bytesRequested == 0)
        return TRUE;
    /* read the remaining bytes directly */
    r = read(dsrc->fd[0],buffer,bytesRequested);
    if (r == -1) {
        /* read error */
        struct pok_exception* ex;
        ex = pok_exception_new();
        ex->kind = pok_ex_net;
        if (errno==EAGAIN || errno==EWOULDBLOCK)
            ex->id = pok_ex_net_wouldblock;
        else if (errno == EINTR)
            ex->id = pok_ex_net_interrupt;
        else
            ex->id = pok_ex_net_unspec;
        *bytesRead = 0;
        return FALSE;
    }
    if (r == 0)
        /* set EOF mode bit */
        dsrc->mode |= DS_MODE_REACH_EOF;
    *bytesRead += r;
    return TRUE;
}
static bool_t pok_data_source_write_primative(int fd,const byte_t* buffer,size_t size,size_t* bytesWritten,bool_t flagError)
{
    /* utilizes a system-call to write data to the specified descriptor; if an exception was generated, FALSE
       is returned and no bytes would have been written */
    ssize_t r;
    if (size == 0) {
        *bytesWritten = 0;
        return TRUE;
    }
    r = write(fd,buffer,size);
    if (r == -1) {
        /* write error */
        if (flagError) {
            struct pok_exception* ex;
            ex = pok_exception_new();
            ex->kind = pok_ex_net;
            if (errno==EAGAIN || errno==EWOULDBLOCK)
                ex->id = pok_ex_net_wouldblock;
            else if (errno == EINTR)
                ex->id = pok_ex_net_interrupt;
            else if (errno == EPIPE)
                ex->id = pok_ex_net_brokenpipe;
            else
                ex->id = pok_ex_net_unspec;
        }
        *bytesWritten = 0;
        return FALSE;
    }
    *bytesWritten = r;
    return TRUE;
}
bool_t pok_data_source_write(struct pok_data_source* dsrc,const byte_t* buffer,size_t size,size_t* bytesWritten)
{
    /* this function attempts to write a data buffer to the device of a specified length; it 
       returns FALSE if an exception was generated; if the data source object is in buffering
       mode, then attempt to write to the buffer; if the buffer becomes full then the data is
       flushed before any extra bytes are written to the device */
    bool_t result;
    if (dsrc->mode & DS_MODE_BUFFER_OUTPUT) {
        ssize_t remain, bufR;
        /* compute remaining bytes; write as much data as possible to the buffer */
        remain = sizeof(dsrc->bufferWrite) - dsrc->szWrite - dsrc->itWrite;
        if (remain > 0) {
            if (size > (size_t)remain) {
                /* reduce used buffer portion to maximize capacity */
                if (dsrc->itWrite > 0) {
                    remain += dsrc->itWrite;
                    if (dsrc->szWrite > 0)
                        memcpy(dsrc->bufferWrite,dsrc->bufferWrite+dsrc->itWrite,dsrc->szWrite);
                    dsrc->itWrite = 0;
                }
                bufR = remain;
            }
            else
                bufR = (ssize_t)size;
            memcpy(dsrc->bufferWrite + dsrc->itWrite + dsrc->szWrite,buffer,bufR);
            dsrc->szWrite += bufR;
            remain -= bufR;
        }
        else
            bufR = 0;
        /* if the buffer became full, flush its contents */
        result = remain <= 0 ? pok_data_source_flush(dsrc) : TRUE;
        *bytesWritten = bufR;
        return result;
    }
    /* the output buffer is non-empty; we must write these bytes before attempting to
       write new data */
    if (dsrc->szWrite > 0) {
        result = pok_data_source_flush(dsrc);
        if (!result || dsrc->szWrite>0) {
            /* flush failed or was incomplete */
            *bytesWritten = 0;
            return result;
        }
    }
    /* attempt to write the supplied buffer to the output device */
    result = pok_data_source_write_primative(dsrc->mode & DS_MODE_FD_BOTH ? dsrc->fd[1] : dsrc->fd[0],buffer,size,bytesWritten,TRUE);
    return result;
}
void pok_data_source_buffering(struct pok_data_source* dsrc,bool_t on)
{
    /* toggle output buffering mode */
    if (on)
        dsrc->mode |= DS_MODE_BUFFER_OUTPUT;
    else
        dsrc->mode &= ~DS_MODE_BUFFER_OUTPUT;
}
bool_t pok_data_source_flush(struct pok_data_source* dsrc)
{
    /* flushes the write buffer; returns FALSE if an exception was generated; the buffer iterator
       and size variable indicate how much (or how little) of the buffer was actually written */
    bool_t result;
    size_t bytesOut;
    result = pok_data_source_write_primative(dsrc->mode & DS_MODE_FD_BOTH ? dsrc->fd[1] : dsrc->fd[0],
        dsrc->bufferWrite+dsrc->itWrite,dsrc->szWrite,&bytesOut,TRUE);
    if (result) {
        dsrc->itWrite += bytesOut;
        dsrc->szWrite -= bytesOut;
        if (dsrc->szWrite == 0)
            /* collapse empty buffer to maximize capacity */
            dsrc->itWrite = 0;
    }
    return result;
}
enum pok_iomode pok_data_source_getmode(struct pok_data_source* dsrc)
{
    /* the first two mode bits correspond to the io-mode */
    return (enum pok_iomode) (dsrc->mode & 0x3);
}
void pok_data_source_free(struct pok_data_source* dsrc)
{
    size_t dummy;
    /* write any remaining bytes in buffer; don't add error to error stack if failure */
    pok_data_source_write_primative(dsrc->mode & DS_MODE_FD_BOTH ? dsrc->fd[1] : dsrc->fd[0],
        dsrc->bufferWrite+dsrc->itWrite,dsrc->szWrite,&dummy,FALSE);
    /* call shutdown syscall if device is socket */
    if (dsrc->mode & DS_MODE_IS_SOCKET)
        shutdown(dsrc->fd[0],SHUT_RDWR);
    /* close file descriptions */
    close(dsrc->fd[0]);
    if (dsrc->mode & DS_MODE_FD_BOTH)
        close(dsrc->fd[1]);
    free(dsrc);
}
void pok_data_source_unread(struct pok_data_source* dsrc,size_t size)
{
    /* this function places unreads the last bytes read by advancing/retreating
       the internal buffer iterators by the specified number of spaces */
    size_t tmp;
    tmp = dsrc->itRead;
    dsrc->itRead -= size;
    dsrc->szRead += size;
    if (dsrc->itRead > tmp) /* overflow on unsigned integer variable */
        dsrc->itRead = 0;
    if (dsrc->szRead > sizeof(dsrc->bufferRead))
        dsrc->szRead = sizeof(dsrc->bufferRead);
}
bool_t pok_data_source_save(struct pok_data_source* dsrc,const byte_t* buffer,size_t size)
{
    /* this function saves data in the data source's output buffer; if not all of the bytes in the
       buffer could be written, then the function returns false and no data was buffered */
    size_t remain = sizeof(dsrc->bufferWrite) - dsrc->szWrite - dsrc->itWrite;
    /* maximize buffer space if it is lacking */
    if (remain < size) {
        if (dsrc->itWrite > 0) {
            remain += dsrc->itWrite;
            if (dsrc->szWrite > 0)
                memcpy(dsrc->bufferWrite,dsrc->bufferWrite+dsrc->itWrite,dsrc->szWrite);
            dsrc->itWrite = 0;
            if (remain < size)
                return FALSE;
        }
        else
            return FALSE;
    }
    memcpy(dsrc->bufferWrite+dsrc->itWrite+dsrc->szWrite,buffer,size);
    dsrc->szWrite += size;
    return TRUE;
}
inline bool_t pok_data_source_readbuf_full(struct pok_data_source* dsrc)
{
    return dsrc->szRead == sizeof(dsrc->bufferRead);
}
inline bool_t pok_data_source_endofcomms(struct pok_data_source* dsrc)
{
    return dsrc->mode & DS_MODE_REACH_EOF;
}
