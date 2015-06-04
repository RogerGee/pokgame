#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pokgame.h"
#include "error.h"

const char* POKGAME_NAME;
const char* TMPDIR;
const char* HOME;

extern int main_test();
extern int net_test1();
extern int graphics_main_test1();

void halt()
{
    char buffer[1024];
    fgets(buffer,sizeof(buffer),stdin);
}

int main(int argc,const char* argv[])
{
    int i;
    char input[256];
    POKGAME_NAME = argv[0];
    printf("Starting pokgame test with cmdline=\"%s",argv[0]);
    for (i = 1;i < argc;++i)
        printf(" %s",argv[i]);
    puts("\"");

    /* load modules */
    pok_exception_load_module();
    pok_game_load_module();

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

    fputs("enter test: ",stdout);
    fgets(input,sizeof(input),stdin);
    i = strlen(input) - 1;
    if (input[i] == '\n')
        input[i] = 0;
    if (strcmp(input,"net") == 0)
        assert(net_test1() == 0);
    else if (strcmp(input,"graphics 1") == 0)
        graphics_main_test1();
    else if (strcmp(input,"main") == 0)
        main_test();

    /* unload modules */
    pok_game_unload_module();
    pok_exception_unload_module();

    return 0;
}
