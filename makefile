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
LIBRARY_REALNAME = libpokgame.so.0.1
LIBRARY_SONAME = libpokgame.so.0
LIBRARY_LINKNAME = libpokgame.so

# output locations
OBJECT_DIRECTORY = obj
OBJECT_DIRECTORY_DEBUG = dobj
OBJECT_DIRECTORY_TEST = tobj

# options
INC = -Isrc
OUT = -o
MACROS = -DPOKGAME_POSIX -DPOKGAME_X11 -DPOKGAME_OPENGL
ifneq "$(or $(MAKE_DEBUG),$(MAKE_TEST))" ""
LIB = -lGL -lX11 -lpthread -ldstructs
MACROS_DEBUG = -DPOKGAME_DEBUG
COMPILE = gcc -c -g -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-parameter -Wno-unused-variable\
		-Wno-unused-function $(MACROS) $(MACROS_DEBUG)
COMPILE_SHARED = $(COMPILE)
LINK = gcc
OBJDIR = $(OBJECT_DIRECTORY_DEBUG)
ifdef MAKE_TEST
DEBUG_BINARY = $(PROGRAM_NAME_TEST)
else
DEBUG_BINARY = $(PROGRAM_NAME_DEBUG)
endif
else
LIB = -lGL -lX11 -lpthread -lpokgame
#MACROS_RELEASE =
COMPILE = gcc -c -O3 -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors $(MACROS)#$(MACROS_RELEASE)
COMPILE_SHARED = $(COMPILE) -fPIC
LINK = gcc -s
OBJDIR = $(OBJECT_DIRECTORY)
BINARY = $(PROGRAM_NAME)
endif

# header file dependencies
PROTOCOL_H = src/protocol.h
TYPES_H = src/types.h
POK_H = src/pok.h $(PROTOCOL_H) $(TYPES_H)
ERROR_H = src/error.h $(TYPES_H)
NET_H = src/net.h $(TYPES_H)
IMAGE_H = src/image.h $(NET_H)
GRAPHICS_H = src/graphics.h $(IMAGE_H)
TILE_H = src/tile.h $(GRAPHICS_H)
SPRITE_H = src/sprite.h $(GRAPHICS_H)
MAP_H = src/map.h $(TILE_H)

# object code files: library objects are used both by clients and 
OBJECTS = graphics.o tile.o sprite.o map.o
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))
OBJECTS_LIB = image.o error.o net.o types.o pok-util.o
OBJECTS_LIB := $(addprefix $(OBJDIR)/,$(OBJECTS_LIB))
ifdef MAKE_TEST
TEST_OBJECTS = main.o nettest.o graphicstest.o
OBJECTS := $(OBJECTS) $(addprefix $(OBJECT_DIRECTORY_TEST)/,$(TEST_OBJECTS))
endif

# general rules
all: $(OBJDIR) $(LIBRARY_REALNAME) $(BINARY)
debug: $(OBJDIR) $(DEBUG_BINARY)
test: $(OBJDIR) $(OBJECT_DIRECTORY_TEST) $(DEBUG_BINARY)

# rules for output binaries: pokgame includes a library that is used
# by both clients and servers; for test builds this code is included
# into the executable itself
$(BINARY): $(OBJECTS)
	$(LINK) $(OUT)$(BINARY) $(OBJECTS) $(LIB)
$(LIBRARY_REALNAME): $(OBJECTS_LIB)
	gcc -shared -Wl,-soname,$(LIBRARY_SONAME) -o $(LIBRARY_REALNAME) $(OBJECTS_LIB)
$(DEBUG_BINARY): $(OBJECTS) $(OBJECTS_LIB)
	$(LINK) $(OUT)$(DEBUG_BINARY) $(OBJECTS) $(OBJECTS_LIB) $(LIB)

# src targets (only for the game client)
$(OBJDIR)/graphics.o: src/graphics.c src/graphics-X-GL.c $(GRAPHICS_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/graphics.o src/graphics.c
$(OBJDIR)/tile.o: src/tile.c $(TILE_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/tile.o src/tile.c
$(OBJDIR)/sprite.o: src/sprite.c $(SPRITE_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/sprite.o src/sprite.c
$(OBJDIR)/map.o: src/map.c $(MAP_H) $(ERROR_H) $(POK_H)
	$(COMPILE) $(OUT)$(OBJDIR)/map.o src/map.c

# src targets for the library
$(OBJDIR)/image.o: src/image.c $(IMAGE_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/image.o src/image.c
$(OBJDIR)/error.o: src/error.c $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/error.o src/error.c
$(OBJDIR)/net.o: src/net.c src/net-posix.c $(NET_H) $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/net.o src/net.c
$(OBJDIR)/types.o: src/types.c $(TYPES_H) $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/types.o src/types.c
$(OBJDIR)/pok-util.o: src/pok-util.c $(POK_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/pok-util.o src/pok-util.c

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
