/* net-win32.c - pokgame */
#include <WinSock2.h>
#include <Windows.h>

/* Note: this file mirrors net-posix.c and as such it is not fully documented; see
   net-posix.c to find documented explainations of the function defined in
   this sub-module; the function names will be similar but identifiers, 
   typenames, ETC. will be different (and of course the implementation will
   vary slightly) */

struct pok_data_source
{
    BOOLEAN bIsSocket;
    BOOLEAN bAtEOF;
    BOOLEAN bDoBuffering;
    BOOLEAN bUsingStandard;

    HANDLE hBoth;
    HANDLE hInput;
    HANDLE hOutput;

    BYTE InputBuffer[4096];
    DWORD InputBufferSize;
    DWORD InputBufferIterator;

    BYTE OutputBuffer[4096];
    DWORD OutputBufferSize;
    DWORD OutputBufferIterator;
};

static void PokDataSourceInit(struct pok_data_source* dsrc)
{
    dsrc->bIsSocket = FALSE;
    dsrc->bAtEOF = FALSE;
    dsrc->bDoBuffering = TRUE;
    dsrc->bUsingStandard = FALSE;
    dsrc->InputBufferSize = 0;
    dsrc->InputBufferIterator = 0;
    dsrc->OutputBufferSize = 0;
    dsrc->OutputBufferIterator = 0;
    dsrc->hBoth = INVALID_HANDLE_VALUE;
    dsrc->hInput = INVALID_HANDLE_VALUE;
    dsrc->hOutput = INVALID_HANDLE_VALUE;
}
struct pok_data_source* pok_data_source_new_standard()
{
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    PokDataSourceInit(dsrc);
    dsrc->bUsingStandard = TRUE;
    dsrc->hInput = GetStdHandle(STD_INPUT_HANDLE);
    dsrc->hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    return dsrc;
}
struct pok_data_source* pok_data_source_new_local_named(const char* name)
{
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    PokDataSourceInit(dsrc);
    /* validate that 'name' follows the correct named pipe format */
    if (strncmp(name, "\\\\.\\pipe\\", 9) != 0) {

        free(dsrc);
        return NULL;
    }
    /* attempt to open named pipe */
    dsrc->hBoth = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (dsrc->hBoth == INVALID_HANDLE_VALUE) {

        free(dsrc);
        return NULL;
    }
    return dsrc;
}
struct pok_data_source* pok_data_source_new_local_anon()
{
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    PokDataSourceInit(dsrc);
    if (!CreatePipe(&dsrc->hInput, &dsrc->hOutput, NULL, 0)) {

        free(dsrc);
        return NULL;
    }
    return dsrc;
}
struct pok_data_source* pok_data_source_new_network(struct pok_network_address* address)
{
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    PokDataSourceInit(dsrc);
    /* TODO */
    (void)address;
    return dsrc;
}
struct pok_data_source* pok_data_source_new_file(const char* filename, enum pok_filemode mode, enum pok_iomode access)
{
    DWORD dwCreatDisp = 0;
    DWORD dwAccess = 0;
    PHANDLE hpFile;
    struct pok_data_source* dsrc = malloc(sizeof(struct pok_data_source));
    if (dsrc == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    PokDataSourceInit(dsrc);
    if (mode == pok_filemode_create_new)
        dwCreatDisp |= CREATE_NEW;
    else if (mode == pok_filemode_create_always)
        dwCreatDisp |= CREATE_ALWAYS;
    else if (mode == pok_filemode_open_existing)
        dwCreatDisp |= OPEN_EXISTING;
    if (access == pok_iomode_full_duplex) {
        dwAccess = GENERIC_READ | GENERIC_WRITE;
        hpFile = &dsrc->hBoth;
    }
    else if (access == pok_iomode_write) {
        dwAccess = GENERIC_WRITE;
        hpFile = &dsrc->hOutput;
    }
    else {
        dwAccess = GENERIC_READ;
        hpFile = &dsrc->hInput;
    }
    *hpFile = CreateFile(
        filename,
        dwAccess,
        0,
        NULL,
        dwCreatDisp,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (*hpFile == NULL) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
            pok_exception_new_ex(pok_ex_net, pok_ex_net_file_permission_denied);
        else if (err == ERROR_FILE_EXISTS)
            pok_exception_new_ex(pok_ex_net, pok_ex_net_file_already_exist);
        else if (err == ERROR_FILE_NOT_FOUND)
            pok_exception_new_ex(pok_ex_net, pok_ex_net_file_does_not_exist);
        else
            pok_exception_new_ex(pok_ex_default, pok_ex_default_undocumented);
        free(dsrc);
        return NULL;
    }
    return dsrc;
}
byte_t* pok_data_source_read(struct pok_data_source* dsrc, size_t bytesRequested, size_t* bytesRead)
{
    DWORD it;
    if (dsrc->bAtEOF) {
        *bytesRead = 0;
        return dsrc->InputBuffer;
    }
    if (bytesRequested > sizeof(dsrc->InputBuffer))
        bytesRequested = sizeof(dsrc->InputBuffer);
    if (bytesRequested > dsrc->InputBufferSize) {
        DWORD r;
        DWORD remain;
        remain = sizeof(dsrc->InputBuffer) - dsrc->InputBufferSize - dsrc->InputBufferIterator;
        if (dsrc->InputBufferIterator>0 && bytesRequested>remain) {
            remain += dsrc->InputBufferSize;
            if (dsrc->InputBufferSize > 0)
                memcpy(dsrc->InputBuffer, dsrc->InputBuffer + dsrc->InputBufferIterator, dsrc->InputBufferSize);
            dsrc->InputBufferIterator = 0;
        }
        if (remain > 0) {
            if (!ReadFile(
                    dsrc->hBoth != INVALID_HANDLE_VALUE ? dsrc->hBoth : dsrc->hInput,
                    dsrc->InputBuffer + dsrc->InputBufferSize + dsrc->InputBufferIterator,
                    remain,
                    &r,
                    NULL)) {
                struct pok_exception* ex;
                ex = pok_exception_new();
                ex->kind = pok_ex_net;
                ex->id = pok_ex_net_unspec;
                *bytesRead = 0;
                return NULL;
            }
            if (r == 0)
                dsrc->bAtEOF = TRUE;
            dsrc->InputBufferSize += r;
        }
    }
    *bytesRead = dsrc->InputBufferSize > bytesRequested ? bytesRequested : dsrc->InputBufferSize;
    it = dsrc->InputBufferIterator;
    dsrc->InputBufferIterator += *bytesRead;
    dsrc->InputBufferSize -= *bytesRead;
    return dsrc->InputBuffer + it;
}
bool_t pok_data_source_read_to_buffer(struct pok_data_source* dsrc, void* buffer, size_t bytesRequested, size_t* bytesRead)
{
    DWORD r;
    if (dsrc->bAtEOF) {
        *bytesRead = 0;
        return TRUE;
    }
    if (dsrc->InputBufferSize > 0) {
        DWORD amt;
        amt = bytesRequested > dsrc->InputBufferSize ? dsrc->InputBufferSize : bytesRequested;
        memcpy(buffer, dsrc->InputBuffer + dsrc->InputBufferIterator, amt);
        buffer = (char*)buffer + amt;
        bytesRequested -= amt;
        dsrc->InputBufferSize -= amt;
        dsrc->InputBufferIterator += amt;
        *bytesRead = amt;
    }
    else
        *bytesRead = 0;
    if (bytesRequested == 0)
        return TRUE;
    if (!ReadFile(
            dsrc->hBoth != INVALID_HANDLE_VALUE ? dsrc->hBoth : dsrc->hInput,
            buffer,bytesRequested,
            &r,
            NULL)) {
        struct pok_exception* ex;
        ex = pok_exception_new();
        ex->kind = pok_ex_net;
        ex->id = pok_ex_net_unspec;
        *bytesRead = 0;
        return FALSE;
    }
    if (r == 0)
        dsrc->bAtEOF = TRUE;
    *bytesRead += r;
    return TRUE;
}
char pok_data_source_peek(struct pok_data_source* dsrc)
{
    size_t bytesRead;
    if (dsrc->InputBufferSize > 0)
        return dsrc->InputBuffer[dsrc->InputBufferIterator];
    if (pok_data_source_read(dsrc, 1, &bytesRead) && bytesRead > 0)
        return dsrc->InputBuffer[--dsrc->InputBufferIterator];
    return (char)-1;
}
char pok_data_source_peek_ex(struct pok_data_source* dsrc,size_t lookahead)
{
    size_t bytesRead;
    if (dsrc->InputBufferSize > lookahead)
        return dsrc->InputBuffer[dsrc->InputBufferIterator + lookahead];
    if (pok_data_source_read(dsrc, lookahead - dsrc->InputBufferSize, &bytesRead) && bytesRead >= lookahead)
        return dsrc->InputBuffer[dsrc->InputBufferIterator -= bytesRead];
    return (char) -1;
}
char pok_data_source_pop(struct pok_data_source* dsrc)
{
    size_t bytesRead;
    if (dsrc->InputBufferSize > 0) {
        --dsrc->InputBufferSize;
        return dsrc->InputBuffer[dsrc->InputBufferIterator++];
    }
    if (pok_data_source_read(dsrc, 1, &bytesRead) && bytesRead > 0)
        return dsrc->InputBuffer[dsrc->InputBufferIterator - 1];
    return (char)-1;
}
static bool_t pok_data_source_write_primative(HANDLE hFile, const byte_t* buffer, size_t size, size_t* bytesWritten, bool_t flagError)
{
    DWORD r;
    if (size == 0) {
        *bytesWritten = 0;
        return TRUE;
    }
    if (!WriteFile(
            hFile,
            buffer,
            size,
            &r,
            NULL)) {
        /* write error */
        if (flagError) {
            struct pok_exception* ex;
            ex = pok_exception_new();
            ex->kind = pok_ex_net;
            ex->id = pok_ex_net_unspec;
        }
        *bytesWritten = 0;
        return FALSE;
    }
    *bytesWritten = r;
    return TRUE;
}
bool_t pok_data_source_write(struct pok_data_source* dsrc, const byte_t* buffer, size_t size, size_t* bytesWritten)
{
    bool_t result;
    if (dsrc->bDoBuffering) {
        DWORD remain, bufR;
        remain = sizeof(dsrc->OutputBuffer) - dsrc->OutputBufferSize - dsrc->OutputBufferIterator;
        if (remain > 0) {
            if (size > remain) {
                if (dsrc->OutputBufferIterator > 0) {
                    remain += dsrc->OutputBufferIterator;
                    if (dsrc->OutputBufferSize > 0)
                        memcpy(dsrc->OutputBuffer, dsrc->OutputBuffer + dsrc->OutputBufferIterator, dsrc->OutputBufferSize);
                    dsrc->OutputBufferIterator = 0;
                }
                bufR = remain;
            }
            else
                bufR = size;
            memcpy(dsrc->OutputBuffer + dsrc->OutputBufferIterator + dsrc->OutputBufferSize, buffer, bufR);
            dsrc->OutputBufferSize += bufR;
            remain -= bufR;
        }
        else
            bufR = 0;
        result = remain <= 0 ? pok_data_source_flush(dsrc) : TRUE;
        *bytesWritten = bufR;
        return result;
    }
    if (dsrc->OutputBufferSize > 0) {
        result = pok_data_source_flush(dsrc);
        if (!result || dsrc->OutputBufferSize > 0) {
            *bytesWritten = 0;
            return result;
        }
    }
    result = pok_data_source_write_primative(
        dsrc->hBoth != INVALID_HANDLE_VALUE ? dsrc->hBoth : dsrc->hOutput,
        buffer,
        size,
        bytesWritten,
        TRUE);
    return result;
}
void pok_data_source_buffering(struct pok_data_source* dsrc, bool_t on)
{
    dsrc->bDoBuffering = on;
}
bool_t pok_data_source_flush(struct pok_data_source* dsrc)
{
    bool_t result;
    size_t bytesOut;
    result = pok_data_source_write_primative(
                dsrc->hBoth != INVALID_HANDLE_VALUE ? dsrc->hBoth : dsrc->hOutput,
                dsrc->OutputBuffer + dsrc->OutputBufferIterator,
                dsrc->OutputBufferSize,
                &bytesOut,
                TRUE);
    if (result) {
        dsrc->OutputBufferIterator += bytesOut;
        dsrc->OutputBufferSize -= bytesOut;
        if (dsrc->OutputBufferSize == 0)
            dsrc->OutputBufferIterator = 0;
    }
    return result;
}
enum pok_iomode pok_data_source_getmode(struct pok_data_source* dsrc)
{
    if (dsrc->hBoth != INVALID_HANDLE_VALUE)
        return pok_iomode_full_duplex;
    else if (dsrc->hInput)
        return pok_iomode_read;
    return pok_iomode_write;
}
void pok_data_source_free(struct pok_data_source* dsrc)
{
    size_t dummy;
    pok_data_source_write_primative(
        dsrc->hBoth != INVALID_HANDLE_VALUE ? dsrc->hBoth : dsrc->hOutput,
        dsrc->OutputBuffer + dsrc->OutputBufferIterator,
        dsrc->OutputBufferSize,
        &dummy,
        FALSE);
    if (dsrc->bIsSocket)
        shutdown((SOCKET)dsrc->hBoth, SD_BOTH);
    if (!dsrc->bUsingStandard) {
        if (dsrc->hBoth != INVALID_HANDLE_VALUE)
            CloseHandle(dsrc->hBoth);
        if (dsrc->hInput != INVALID_HANDLE_VALUE)
            CloseHandle(dsrc->hInput);
        if (dsrc->hOutput != INVALID_HANDLE_VALUE)
            CloseHandle(dsrc->hOutput);
    }
    free(dsrc);
}
void pok_data_source_unread(struct pok_data_source* dsrc, size_t size)
{
    DWORD tmp;
    tmp = dsrc->InputBufferIterator;
    dsrc->InputBufferIterator -= size;
    dsrc->InputBufferSize += size;
    if (dsrc->InputBufferIterator > tmp)
        dsrc->InputBufferIterator = 0;
    if (dsrc->InputBufferSize > sizeof(dsrc->InputBuffer))
        dsrc->InputBufferSize = sizeof(dsrc->InputBuffer);
}
bool_t pok_data_source_save(struct pok_data_source* dsrc, const byte_t* buffer, size_t size)
{
    size_t remain = sizeof(dsrc->OutputBuffer) - dsrc->OutputBufferSize - dsrc->OutputBufferIterator;
    if (remain < size) {
        if (dsrc->OutputBufferIterator > 0) {
            remain += dsrc->OutputBufferIterator;
            if (dsrc->OutputBufferSize > 0)
                memcpy(dsrc->OutputBuffer, dsrc->OutputBuffer + dsrc->OutputBufferIterator, dsrc->OutputBufferSize);
            dsrc->OutputBufferIterator = 0;
            if (remain < size)
                return FALSE;
        }
        else
            return FALSE;
    }
    memcpy(dsrc->OutputBuffer + dsrc->OutputBufferIterator + dsrc->OutputBufferSize, buffer, size);
    dsrc->OutputBufferSize += size;
    return TRUE;
}
inline bool_t pok_data_source_readbuf_full(struct pok_data_source* dsrc)
{
    return dsrc->InputBufferSize == sizeof(dsrc->InputBuffer);
}
inline bool_t pok_data_source_endofcomms(struct pok_data_source* dsrc)
{
    return dsrc->bAtEOF;
}
