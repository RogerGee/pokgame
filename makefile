################################################################################
# Makefile for 'pokgame' #######################################################
## targets: GNU/Linux with X11 and OpenGL ######################################
################################################################################
.PHONY: debug test clean install uninstall

ifeq ($(MAKECMDGOALS),debug)
MAKE_DEBUG = yes
else
ifeq ($(MAKECMDGOALS),test)
MAKE_TEST = yes
endif
endif

# output files
PROGRAM_NAME = pokgame
PROGRAM_NAME_DEBUG = pokgame-debug
PROGRAM_NAME_TEST = pokgame-test

# output locations
OBJECT_DIRECTORY = obj
OBJECT_DIRECTORY_DEBUG = dobj
OBJECT_DIRECTORY_TEST = tobj

# options
LIB = -lGL -lX11 -lpthread -ldstructs
INC = -Isrc
OUT = -o
MACROS = -DPOKGAME_POSIX -DPOKGAME_X11 -DPOKGAME_OPENGL
ifneq "$(or $(MAKE_DEBUG),$(MAKE_TEST))" ""
MACROS_DEBUG = -DPOKGAME_DEBUG
COMPILE = gcc -c -g -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-parameter -Wno-unused-variable\
		-Wno-unused-function $(MACROS) $(MACROS_DEBUG)
LINK = gcc
OBJDIR = $(OBJECT_DIRECTORY_DEBUG)
ifdef MAKE_TEST
BINARY = $(PROGRAM_NAME_TEST)
else
BINARY = $(PROGRAM_NAME_DEBUG)
endif
else
#MACROS_RELEASE =
COMPILE = gcc -c -O3 -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors $(MACROS)#$(MACROS_RELEASE)
LINK = gcc -s
OBJDIR = $(OBJECT_DIRECTORY)
BINARY = $(PROGRAM_NAME)
endif

# header file dependencies
TYPES_H = src/types.h
ERROR_H = src/error.h $(TYPES_H)
NET_H = src/net.h $(TYPES_H)
IMAGE_H = src/image.h $(NET_H)
GRAPHICS_H = src/graphics.h $(IMAGE_H)
TILE_H = src/tile.h $(GRAPHICS_H)
SPRITE_H = src/sprite.h $(GRAPHICS_H)

# object code files
OBJECTS = error.o net.o types.o image.o graphics.o tile.o sprite.o
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))
ifdef MAKE_TEST
TEST_OBJECTS = main.o nettest.o graphicstest.o
OBJECTS := $(OBJECTS) $(addprefix $(OBJECT_DIRECTORY_TEST)/,$(TEST_OBJECTS))
endif

# general rules
all: $(OBJDIR) $(BINARY)
debug: $(OBJDIR) $(BINARY)
test: $(OBJDIR) $(OBJECT_DIRECTORY_TEST) $(BINARY)

# rule for binary
$(BINARY): $(OBJECTS)
	$(LINK) $(OUT)$(BINARY) $(OBJECTS) $(LIB)

# src targets
$(OBJDIR)/types.o: src/types.c $(TYPES_H)
	$(COMPILE) $(OUT)$(OBJDIR)/types.o src/types.c
$(OBJDIR)/net.o: src/net.c src/net-posix.c $(NET_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/net.o src/net.c
$(OBJDIR)/image.o: src/image.c $(IMAGE_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/image.o src/image.c
$(OBJDIR)/error.o: src/error.c $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/error.o src/error.c
$(OBJDIR)/graphics.o: src/graphics.c src/graphics-X-GL.c $(GRAPHICS_H)
	$(COMPILE) $(OUT)$(OBJDIR)/graphics.o src/graphics.c
$(OBJDIR)/tile.o: src/tile.c $(TILE_H)
	$(COMPILE) $(OUT)$(OBJDIR)/tile.o src/tile.c
$(OBJDIR)/sprite.o: src/sprite.c $(SPRITE_H)
	$(COMPILE) $(OUT)$(OBJDIR)/sprite.o src/sprite.c

# test targets
$(OBJECT_DIRECTORY_TEST)/main.o: test/main.c
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/main.o test/main.c
$(OBJECT_DIRECTORY_TEST)/nettest.o: test/nettest.c $(NET_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/nettest.o test/nettest.c
$(OBJECT_DIRECTORY_TEST)/graphicstest.o: test/graphicstest.c $(GRAPHICS_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/graphicstest.o test/graphicstest.c

# other targets
$(OBJDIR):
	@mkdir $(OBJDIR)
$(OBJECT_DIRECTORY_TEST):
	@mkdir $(OBJECT_DIRECTORY_TEST)

# other rules
