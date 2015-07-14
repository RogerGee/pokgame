/* menu.h - pokgame */
#ifndef POKGAME_MENU_H
#define POKGAME_MENU_H
#include "graphics.h"
#include "image.h"
#include "protocol.h"

/* define glyphs: the glyphs are accessible by any printable ASCII value */
void pok_glyphs_load();
void pok_glyphs_unload();
struct pok_image* pok_glyph(int c);

/* pok_text_context: interface for rendering text on the screen; this type is
   mainly used by the menu implementation rather than being used directly */
struct pok_text_context
{
    int32_t x, y;        /* screen location to render text */
    struct pok_size region; /* dimensionalized viewable text region */
    bool_t finished;     /* if zero, then the menu has not finished all its sequences */

    int32_t curline;     /* index of top line to render within viewable text region */
    uint32_t curcount;   /* number of total glyphs in current text sequence */
    uint32_t progress;   /* number of glyphs to render at given point; animation progress */

    char** lines;        /* line buffers */
    size_t linecnt;      /* line count */
    size_t linealloc;    /* line buffer allocation */

    uint32_t updateTicks;    /* number of elapsed update ticks */
    uint32_t updateTicksAmt; /* number of required update ticks before update is applied */
};
void pok_text_context_init(struct pok_text_context* text);
void pok_text_context_delete(struct pok_text_context* text);
void pok_text_context_assign(struct pok_text_context* text,const char* message,const struct pok_size* region);
bool_t pok_text_context_update(struct pok_text_context* text,uint32_t ticks);
bool_t pok_text_context_next(struct pok_text_context* text);
void pok_text_context_render(struct pok_graphics_subsystem* sys,struct pok_text_context* text);

/* pok_text_input: provides text input functionality; implemented as a subclass of a text
   rendering context, the input object displays a prompt and then displays user input */
struct pok_text_input
{
    struct pok_text_context base; /* base structure: used to render prompt and echo input */

    int32_t iniLine; /* initial insert line */
    int32_t iniPos;  /* initial insert position */
    int32_t line; /* edit line */
    int32_t pos;  /* edit position (characters inserted here) */

    bool_t finished; /* if non-zero, then the user has finished entering input */
};
void pok_text_input_init(struct pok_text_input* ti);
void pok_text_input_delete(struct pok_text_input* ti);
void pok_text_input_assign(struct pok_text_input* ti,const char* prompt,const struct pok_size* region);
void pok_text_input_reset(struct pok_text_input* ti);
void pok_text_input_entry(struct pok_text_input* ti,char c);
bool_t pok_text_input_ctrl_key(struct pok_text_input* ti,enum pok_input_key key);
void pok_text_input_read(struct pok_text_input* ti,struct pok_string* buffer);
void pok_text_input_render(struct pok_graphics_subsystem* sys,struct pok_text_input* ti);

/* pok_menu: represents a base class for menu functionality */
struct pok_menu
{
    uint8_t padding;              /* number of pixels to pad menu border */
    union pixel fillColor;        /* fill color for menu background */
    struct pok_size size;         /* size of menu in screen pixels */
    struct pok_location pos;      /* position of menu in screen pixels */
};
void pok_menu_init(struct pok_menu* menu,const struct pok_size* size,union pixel fillColor);

struct pok_message_menu
{

    struct pok_text_context text; /* menu display text */
};

struct pok_input_menu
{

    struct pok_text_input input;  /* menu input object */
};

#endif
