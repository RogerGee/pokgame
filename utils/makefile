################################################################################
# Makefile for pokgame utilities ###############################################
################################################################################

all: lib.o tilemaker spritemaker

lib.o: lib.c lib.h
	$(CC) -c -Werror -olib.o lib.c

tilemaker: tilemaker.c lib.h
	$(CC) -Werror -otilemaker tilemaker.c -ldstructs lib.o

spritemaker: spritemaker.c lib.h
	$(CC) -Werror -ospritemaker spritemaker.c -ldstructs lib.o
