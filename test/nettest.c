#include <stdio.h>
#include <string.h>
#include "net.h"
#include "error.h"

extern const char* TMPDIR;
extern void halt();

/* net_test1() - test basic file IO functionality */
int net_test1()
{
    size_t len;
    char fname[128];
    struct pok_data_source* writer;
    struct pok_data_source* reader;

    len = strlen(TMPDIR);
    strncpy(fname,TMPDIR,sizeof(fname));
    strncpy(fname+len,"/pokgame-net-out",sizeof(fname)-len);

    writer = pok_data_source_new_file(fname,pok_filemode_create_always,pok_iomode_write);
    if (writer == NULL) {
        printf("failed to open %s for writing\n",fname);
        pok_exception_pop();
        pok_data_source_free(writer);
        return 1;
    }
    else {
        pok_data_stream_write_uint64(writer,33);
        pok_data_stream_write_uint32(writer,33);
        pok_data_stream_write_uint16(writer,33);
        pok_data_stream_write_byte(writer,33);
        pok_data_source_free(writer);
        printf("wrote to %s\n",fname);
    }

    strncpy(fname+len,"/pokgame-net-in",sizeof(fname)-len);

    printf(" - add content to %s ",fname);
    halt();
    reader = pok_data_source_new_file(fname,pok_filemode_open_existing,pok_iomode_read);
    if (reader == NULL) {
        printf("failed to open %s for reading\n",fname);
        pok_exception_pop();
        return 1;
    }
    else {
        uint32_t var;
        const struct pok_exception* ex;
        printf("reading uint32s from file:\n");
        while (pok_data_stream_read_uint32(reader,&var))
            printf("%ud\n",var);
        pok_data_source_free(reader);
        ex = pok_exception_pop();
        if (ex->kind == pok_ex_net && ex->id == pok_ex_net_endofcomms)
            printf("found end-of-file\n");
        else {
            printf("received another error: kind=%d, id=%d\n",ex->kind,ex->id);
            return 1;
        }
    }
    return 0;
}
