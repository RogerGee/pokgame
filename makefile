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
LIB = -lGL -lX11 -lpthread
LIBRARY_LIB = -ldstructs -lpng
ifneq "$(or $(MAKE_DEBUG),$(MAKE_TEST))" ""
MACROS_DEBUG = -DPOKGAME_DEBUG
COMPILE = gcc -c -g -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-parameter -Wno-unused-variable\
		-Wno-unused-function -std=gnu99 $(MACROS) $(MACROS_DEBUG)
COMPILE_SHARED = $(COMPILE)
LINK = gcc
ifdef MAKE_TEST
DEBUG_BINARY = $(PROGRAM_NAME_TEST)
MACROS_DEBUG := $(MACROS_DEBUG) -DPOKGAME_TEST
OBJDIR = $(OBJECT_DIRECTORY_TEST)
else
DEBUG_BINARY = $(PROGRAM_NAME_DEBUG)
OBJDIR = $(OBJECT_DIRECTORY_DEBUG)
endif
else
LIB := $(LIB) -lpokgame
#MACROS_RELEASE =
COMPILE = gcc -c -O3 -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -std=gnu99 $(MACROS)#$(MACROS_RELEASE)
COMPILE_SHARED = $(COMPILE) -fPIC
LINK = gcc -s
OBJDIR = $(OBJECT_DIRECTORY)
BINARY = $(PROGRAM_NAME)
endif

# header file dependencies
TYPES_H = src/types.h
CONFIG_H = src/config.h
PROTOCOL_H = src/protocol.h $(TYPES_H)
STANDARD_H = src/standard1.h $(TYPES_H)
PARSER_H = src/parser.h $(TYPES_H)
POK_H = src/pok.h $(PROTOCOL_H)
ERROR_H = src/error.h $(TYPES_H)
NET_H = src/net.h $(TYPES_H)
NETOBJ_H = src/netobj.h $(NET_H) $(PROTOCOL_H)
IMAGE_H = src/image.h $(NETOBJ_H)
GRAPHICS_H = src/graphics.h $(NETOBJ_H) $(IMAGE_H)
EFFECT_H = src/effect.h $(GRAPHICS_H)
TILE_H = src/tile.h $(NETOBJ_H)
TILEMAN_H = src/tileman.h $(NETOBJ_H) $(IMAGE_H) $(GRAPHICS_H) $(TILE_H)
SPRITEMAN_H = src/spriteman.h $(NETOBJ_H) $(IMAGE_H) $(GRAPHICS_H)
MAP_H = src/map.h $(NETOBJ_H) $(TILE_H) $(PROTOCOL_H)
MAP_CONTEXT_H = src/map-context.h $(MAP_H) $(GRAPHICS_H)
CHARACTER_H = src/character.h $(NETOBJ_H)
CHARACTER_CONTEXT_H = src/character-context.h $(MAP_CONTEXT_H) $(SPRITEMAN_H) $(CHARACTER_H)
POKGAME_H = src/pokgame.h $(NET_H) $(GRAPHICS_H) $(TILEMAN_H) $(SPRITEMAN_H) $(MAP_CONTEXT_H) $(CHARACTER_CONTEXT_H) $(EFFECT_H)
DEFAULT_H = src/default.h $(POKGAME_H)

# object code files: library objects are used both by the game engine and game versions
OBJECTS = pokgame.o graphics.o effect.o tileman.o spriteman.o map-context.o character-context.o update-proc.o io-proc.o default.o \
          standard.o
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))
OBJECTS_LIB = image.o error.o net.o netobj.o types.o parser.o pok-util.o tile.o map.o character.o
OBJECTS_LIB := $(addprefix $(OBJDIR)/,$(OBJECTS_LIB))
ifdef MAKE_TEST
TEST_OBJECTS = main.o maintest.o nettest.o graphicstest1.o
OBJECTS := $(OBJECTS) $(addprefix $(OBJECT_DIRECTORY_TEST)/,$(TEST_OBJECTS))
endif

# general rules
all: $(OBJDIR) $(LIBRARY_REALNAME) $(BINARY)
debug: $(OBJDIR) $(DEBUG_BINARY)
test: $(OBJDIR) $(DEBUG_BINARY)

# rules for output binaries: pokgame includes a library that is used
# by both clients and servers; for test builds this code is included
# into the executable itself
$(BINARY): $(OBJECTS)
	$(LINK) $(OUT)$(BINARY) $(OBJECTS) $(LIB)
$(LIBRARY_REALNAME): $(OBJECTS_LIB)
	gcc -shared -Wl,-soname,$(LIBRARY_SONAME) -o $(LIBRARY_REALNAME) $(OBJECTS_LIB) $(LIBRARY_LIB)
$(DEBUG_BINARY): $(OBJECTS) $(OBJECTS_LIB)
	$(LINK) $(OUT)$(DEBUG_BINARY) $(OBJECTS) $(OBJECTS_LIB) $(LIB) $(LIBRARY_LIB)

# src targets (only for the game engine)
$(OBJDIR)/pokgame.o: src/pokgame.c src/pokgame-posix.c $(POKGAME_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/pokgame.o src/pokgame.c
$(OBJDIR)/graphics.o: src/graphics.c src/graphics-X-GL.c src/graphics-GL.c $(GRAPHICS_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJDIR)/graphics.o src/graphics.c
$(OBJDIR)/effect.o: src/effect.c src/effect-GL.c $(EFFECT_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/effect.o src/effect.c
$(OBJDIR)/tileman.o: src/tileman.c $(TILEMAN_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/tileman.o src/tileman.c
$(OBJDIR)/spriteman.o: src/spriteman.c $(SPRITEMAN_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJDIR)/spriteman.o src/spriteman.c
$(OBJDIR)/map-context.o: src/map-context.c $(MAP_CONTEXT_H) $(PROTOCOL_H) $(POKGAME_H)
	$(COMPILE) $(OUT)$(OBJDIR)/map-context.o src/map-context.c
$(OBJDIR)/character-context.o: src/character-context.c $(CHARACTER_CONTEXT_H) $(ERROR) $(POKGAME_H)
	$(COMPILE) $(OUT)$(OBJDIR)/character-context.o src/character-context.c
$(OBJDIR)/update-proc.o: src/update-proc.c $(POKGAME_H) $(PROTOCOL_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/update-proc.o src/update-proc.c
$(OBJDIR)/io-proc.o: src/io-proc.c $(POKGAME_H) $(ERROR_H) $(PROTOCOL_H) $(DEFAULT_H)
	$(COMPILE) $(OUT)$(OBJDIR)/io-proc.o src/io-proc.c
$(OBJDIR)/default.o: src/default.c $(DEFAULT_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/default.o src/default.c
$(OBJDIR)/standard.o: src/standard1.c $(STANDARD_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/standard.o src/standard1.c

# src targets for the library
$(OBJDIR)/image.o: src/image.c $(IMAGE_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJDIR)/image.o src/image.c
$(OBJDIR)/error.o: src/error.c $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/error.o src/error.c
$(OBJDIR)/net.o: src/net.c src/net-posix.c $(NET_H) $(ERROR_H) $(PARSER_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/net.o src/net.c
$(OBJDIR)/netobj.o: src/netobj.c $(NETOBJ_H) $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/netobj.o src/netobj.c
$(OBJDIR)/types.o: src/types.c $(TYPES_H) $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/types.o src/types.c
$(OBJDIR)/parser.o: src/parser.c $(PARSER_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/parser.o src/parser.c
$(OBJDIR)/pok-util.o: src/pok-util.c $(POK_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/pok-util.o src/pok-util.c
$(OBJDIR)/tile.o: src/tile.c $(TILE_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJDIR)/tile.o src/tile.c
$(OBJDIR)/map.o: src/map.c $(MAP_H) $(ERROR_H) $(POK_H) $(PARSER_H)
	$(COMPILE) $(OUT)$(OBJDIR)/map.o src/map.c
$(OBJDIR)/character.o: src/character.c $(CHARACTER_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/character.o src/character.c

# test targets
$(OBJDIR)/main.o: test/main.c
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/main.o test/main.c
$(OBJDIR)/maintest.o: test/maintest.c $(POKGAME_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/maintest.o test/maintest.c
$(OBJDIR)/nettest.o: test/nettest.c $(NET_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/nettest.o test/nettest.c
$(OBJDIR)/graphicstest1.o: test/graphicstest1.c $(GRAPHICS_H) $(TILEMAN_H) $(MAP_CONTEXT_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/graphicstest1.o test/graphicstest1.c

# other targets
$(OBJDIR):
	@mkdir $(OBJDIR)

# other rules
