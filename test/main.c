#include <stdio.h>
#include "graphics.h"

const char* POKGAME_NAME;

int main(int argc,const char* argv[])
{    
    int i;
    POKGAME_NAME = argv[0];
    printf("Starting pokgame test with cmdline=\"%s",argv[0]);
    for (i = 1;i < argc;++i)
        printf(" %s",argv[i]);
    puts("\"");
    return 0;
}
