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
		-Wno-unused-function -std=gnu99 $(MACROS) $(MACROS_DEBUG)
COMPILE_SHARED = $(COMPILE)
LINK = gcc
OBJDIR = $(OBJECT_DIRECTORY_DEBUG)
ifdef MAKE_TEST
DEBUG_BINARY = $(PROGRAM_NAME_TEST)
MACROS_DEBUG := $(MACROS_DEBUG) -DPOKGAME_TEST
else
DEBUG_BINARY = $(PROGRAM_NAME_DEBUG)
endif
else
LIB = -lGL -lX11 -lpthread -lpokgame
#MACROS_RELEASE =
COMPILE = gcc -c -O3 -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -std=gnu99 $(MACROS)#$(MACROS_RELEASE)
COMPILE_SHARED = $(COMPILE) -fPIC
LINK = gcc -s
OBJDIR = $(OBJECT_DIRECTORY)
BINARY = $(PROGRAM_NAME)
endif

# header file dependencies
PROTOCOL_H = src/protocol.h
TYPES_H = src/types.h
PARSER_H = src/parser.h $(TYPES_H)
POK_H = src/pok.h $(PROTOCOL_H) $(TYPES_H)
ERROR_H = src/error.h $(TYPES_H)
NET_H = src/net.h $(TYPES_H)
IMAGE_H = src/image.h $(NET_H)
GRAPHICS_H = src/graphics.h $(NET_H) $(IMAGE_H)
TILE_H = src/tile.h $(NET_H)
TILEMAN_H = src/tileman.h $(NET_H) $(IMAGE_H) $(GRAPHICS_H) $(TILE_H)
SPRITEMAN_H = src/spriteman.h $(NET_H) $(IMAGE_H) $(GRAPHICS_H)
MAP_H = src/map.h $(NET_H) $(TILE_H)
MAP_RENDER_H = src/map-render.h $(MAP_H) $(GRAPHICS_H)
CHARACTER_H = src/character.h $(NET_H)
CHARACTER_RENDER_H = src/character-render.h $(MAP_RENDER_H) $(SPRITEMAN_H) $(CHARACTER_H)
POKGAME_H = src/pokgame.h $(NET_H) $(GRAPHICS_H) $(TILEMAN_H) $(SPRITEMAN_H) $(MAP_RENDER_H) $(CHARACTER_RENDER_H)

# object code files: library objects are used both by clients and version servers
OBJECTS = pokgame.o graphics.o tileman.o spriteman.o map-render.o character-render.o update-proc.o io-proc.o
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))
OBJECTS_LIB = image.o error.o net.o types.o parser.o pok-util.o tile.o map.o character.o
OBJECTS_LIB := $(addprefix $(OBJDIR)/,$(OBJECTS_LIB))
ifdef MAKE_TEST
TEST_OBJECTS = main.o maintest.o nettest.o graphicstest1.o
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
$(OBJDIR)/pokgame.o: src/pokgame.c src/pokgame-posix.c $(POKGAME_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/pokgame.o src/pokgame.c
$(OBJDIR)/graphics.o: src/graphics.c src/graphics-X-GL.c src/graphics-GL.c $(GRAPHICS_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJDIR)/graphics.o src/graphics.c
$(OBJDIR)/tileman.o: src/tileman.c $(TILEMAN_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/tileman.o src/tileman.c
$(OBJDIR)/spriteman.o: src/spriteman.c $(SPRITEMAN_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/spriteman.o src/spriteman.c
$(OBJDIR)/map-render.o: src/map-render.c $(MAP_RENDER_H) $(PROTOCOL_H) $(POKGAME_H)
	$(COMPILE) $(OUT)$(OBJDIR)/map-render.o src/map-render.c
$(OBJDIR)/character-render.o: src/character-render.c $(CHARACTER_RENDER_H) $(ERROR)
	$(COMPILE) $(OUT)$(OBJDIR)/character-render.o src/character-render.c

$(OBJDIR)/update-proc.o: src/update-proc.c $(POKGAME_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/update-proc.o src/update-proc.c
$(OBJDIR)/io-proc.o: src/io-proc.c $(POKGAME_H) $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/io-proc.o src/io-proc.c

# src targets for the library
$(OBJDIR)/image.o: src/image.c $(IMAGE_H) $(ERROR_H) $(PROTOCOL_H)
	$(COMPILE) $(OUT)$(OBJDIR)/image.o src/image.c
$(OBJDIR)/error.o: src/error.c $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/error.o src/error.c
$(OBJDIR)/net.o: src/net.c src/net-posix.c $(NET_H) $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/net.o src/net.c
$(OBJDIR)/types.o: src/types.c $(TYPES_H) $(ERROR_H)
	$(COMPILE_SHARED) $(OUT)$(OBJDIR)/types.o src/types.c
$(OBJDIR)/parser.o: src/parser.c $(PARSER_H) $(ERROR_H)
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
$(OBJECT_DIRECTORY_TEST)/main.o: test/main.c
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/main.o test/main.c
$(OBJECT_DIRECTORY_TEST)/maintest.o: test/maintest.c $(POKGAME_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/maintest.o test/maintest.c
$(OBJECT_DIRECTORY_TEST)/nettest.o: test/nettest.c $(NET_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/nettest.o test/nettest.c
$(OBJECT_DIRECTORY_TEST)/graphicstest1.o: test/graphicstest1.c $(GRAPHICS_H) $(TILEMAN_H) $(MAP_RENDER_H) $(ERROR_H)
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/graphicstest1.o test/graphicstest1.c

# other targets
$(OBJDIR):
	@mkdir $(OBJDIR)
$(OBJECT_DIRECTORY_TEST):
	@mkdir $(OBJECT_DIRECTORY_TEST)

# other rules
