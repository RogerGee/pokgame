#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "error.h"

const char* POKGAME_NAME;
const char* TMPDIR;
const char* HOME;

extern int net_test1();
extern int graphics_main_test();

void halt()
{
    char buffer[1024];
    fgets(buffer,sizeof(buffer),stdin);
}

int main(int argc,const char* argv[])
{
    int i;
    POKGAME_NAME = argv[0];
    printf("Starting pokgame test with cmdline=\"%s",argv[0]);
    for (i = 1;i < argc;++i)
        printf(" %s",argv[i]);
    puts("\"");

    /* load modules */
    pok_exception_load_module();

    /* find a temporary directory */
    if ((TMPDIR = getenv("TEMPDIR")) == NULL && (TMPDIR = getenv("TMPDIR")) == NULL && (TMPDIR = getenv("TMP")) == NULL) {
        fprintf(stderr,"%s: couldn't find temporary directory in program environment\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* find home directory */
    if ((HOME = getenv("HOME")) == NULL) {
        fprintf(stderr,"%s: couldn't find home directory\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc > 1) {
        if (strcmp(argv[1],"net") == 0) {
            assert(net_test1() == 0);
        }
    }
    else
        /* else do the main application test */
        graphics_main_test();

    /* unload modules */
    pok_exception_unload_module();

    return 0;
}