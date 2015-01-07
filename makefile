################################################################################
# Makefile for 'pokgame' #######################################################
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
MACROS = -DPOKGAME_POSIX -DPOKGAME_X11
ifneq "$(or $(MAKE_DEBUG),$(MAKE_TEST))" ""
MACROS_DEBUG = -DPOKGAME_DEBUG
COMPILE = gcc -c -g -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable $(MACROS) $(MACROS_DEBUG)
LINK = gcc
OBJDIR = $(OBJECT_DIRECTORY_DEBUG)
ifdef MAKE_TEST
BINARY = $(PROGRAM_NAME_TEST)
else
BINARY = $(PROGRAM_NAME_DEBUG)
endif
else
#MACROS_RELEASE =
COMPILE = gcc -c -O3 -Wall -pedantic-errors -Werror -Wextra -Wshadow -Wfatal-errors -Wno-unused-variable $(MACROS)#$(MACROS_RELEASE)
LINK = gcc -s
OBJDIR = $(OBJECT_DIRECTORY)
BINARY = $(PROGRAM_NAME)
endif

# header file dependencies
TYPES_H = src/types.h
GRAPHICS_H = src/graphics.h $(TYPES_H)
NET_H = src/net.h $(TYPES_H)

# object code files
OBJECTS = graphics.o net.o error.o
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))
ifdef MAKE_TEST
TEST_OBJECTS = main.o
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
$(OBJDIR)/graphics.o: src/graphics.c $(GRAPHICS_H)
	$(COMPILE) $(OUT)$(OBJDIR)/graphics.o src/graphics.c
$(OBJDIR)/net.o: src/net.c $(NET_H)
	$(COMPILE) $(OUT)$(OBJDIR)/net.o src/net.c
$(OBJDIR)/error.o: src/error.c $(ERROR_H)
	$(COMPILE) $(OUT)$(OBJDIR)/error.o src/error.c

# test targets
$(OBJECT_DIRECTORY_TEST)/main.o: test/main.c
	$(COMPILE) $(INC) $(OUT)$(OBJECT_DIRECTORY_TEST)/main.o test/main.c

# other targets
$(OBJDIR):
	@mkdir $(OBJDIR)
$(OBJECT_DIRECTORY_TEST):
	@mkdir $(OBJECT_DIRECTORY_TEST)

# other rules
