/* menu.h - pokgame */
#ifndef POKGAME_MENU_H
#define POKGAME_MENU_H
#include "graphics.h"
#include "image.h"
#include "protocol.h"

/* macros for color strings */
#define POK_TEXT_COLOR_WHITE  "\033\001"
#define POK_TEXT_COLOR_BLACK  "\033\002"
#define POK_TEXT_COLOR_GRAY   "\033\003"
#define POK_TEXT_COLOR_BLUE   "\033\004"
#define POK_TEXT_COLOR_RED    "\033\005"
#define POK_TEXT_COLOR_PURPLE "\033\006"
#define POK_TEXT_COLOR_YELLOW "\033\007"
#define POK_TEXT_COLOR_ORANGE "\033\010"
#define POK_TEXT_COLOR_GREEN  "\033\011"

/* define glyphs: the glyphs are accessible by any printable ASCII value */
void pok_glyphs_load();
void pok_glyphs_unload();
struct pok_image* pok_glyph(int c);

/* pok_text_context: interface for rendering text on the screen; this type is
   mainly used by the menu implementation rather than being used directly */
struct pok_text_context
{
    int32_t x, y;        /* screen location to render text */
    float textSize;      /* multiplier for font glyph dimensions */
    struct pok_size region; /* dimensionalized viewable text region */
    bool_t finished;     /* if non-zero, then the menu has finished all its sequences and the user has attempted to continue */

    int32_t curline;     /* index of top line to render within viewable text region */
    uint32_t curcount;   /* number of total glyphs in current text sequence */
    uint32_t progress;   /* number of glyphs to render at given point; animation progress */
    uint32_t cprogress;  /* color buffer progress */

    char** lines;        /* line buffers */
    size_t linecnt;      /* line count */
    size_t linealloc;    /* line buffer allocation */

    int8_t defaultColor; /* default text color */
    int8_t* colorbuf;    /* stores 'pok_menu_color' flags that represent color information for each character position */
    size_t coloralloc;   /* color buffer allocation */

    uint32_t updateTicks;    /* number of elapsed update ticks */
    uint32_t updateTicksAmt; /* number of required update ticks before update is applied */
};
void pok_text_context_init(struct pok_text_context* text,const struct pok_size* region);
void pok_text_context_delete(struct pok_text_context* text);
void pok_text_context_assign(struct pok_text_context* text,const char* message);
void pok_text_context_reset(struct pok_text_context* text);
bool_t pok_text_context_update(struct pok_text_context* text,uint32_t ticks);
void pok_text_context_cancel_update(struct pok_text_context* text);
void pok_text_context_next(struct pok_text_context* text);
void pok_text_context_render(struct pok_text_context* text);

/* pok_text_input: provides text input functionality; implemented as a subclass of a text
   rendering context, the input object displays a prompt and then displays user input */
struct pok_text_input
{
    struct pok_text_context base; /* base structure: used to render prompt and echo input */

    int32_t iniLine; /* initial insert line */
    int32_t iniPos;  /* initial insert position */
    int32_t line; /* edit line */
    int32_t pos;  /* edit position (characters inserted here) */

    int8_t cursorColor; /* 'pok_menu_color' flag representing cursor fill color*/
    bool_t accepting; /* if non-zero, then the text input is ready to accept input */
    bool_t finished; /* if non-zero, then the user has finished entering input */
};
void pok_text_input_init(struct pok_text_input* ti,const struct pok_size* region);
void pok_text_input_delete(struct pok_text_input* ti);
void pok_text_input_assign(struct pok_text_input* ti,const char* prompt);
void pok_text_input_reset(struct pok_text_input* ti);
void pok_text_input_entry(struct pok_text_input* ti,char c);
bool_t pok_text_input_ctrl_key(struct pok_text_input* ti,enum pok_input_key key);
bool_t pok_text_input_update(struct pok_text_input* ti,uint32_t ticks);
void pok_text_input_read(struct pok_text_input* ti,struct pok_string* buffer);
void pok_text_input_render(struct pok_text_input* ti);

/* pok_menu: represents a base class for menu functionality */
struct pok_menu
{
    bool_t active;                /* if non-zero, then the menu should be displayed */
    bool_t focused;               /* if non-zero, then the menu should receive key input messages */
    uint8_t padding;              /* number of pixels to pad menu (border area size) */
    int8_t fillColor;             /* 'pok_menu_color' flag for menu background */
    int8_t borderColor;           /* 'pok_menu_color' flag for border color */
    struct pok_size size;         /* size of menu in screen pixels */
    struct pok_location pos;      /* position of menu in screen pixels */
};

/* pok_message_menu: represents a simple text display menu that always appears
   at the bottom of the window in the standard way */
struct pok_message_menu
{
    struct pok_menu base;
    struct pok_text_context text; /* menu display text */
};
void pok_message_menu_init(struct pok_message_menu* menu,const struct pok_graphics_subsystem* sys);
void pok_message_menu_delete(struct pok_message_menu* menu);
void pok_message_menu_ctrl_key(struct pok_message_menu* menu,enum pok_input_key key);
void pok_message_menu_activate(struct pok_message_menu* menu,const char* message);
void pok_message_menu_deactivate(struct pok_message_menu* menu);
void pok_message_menu_render(struct pok_message_menu* menu);

/* pok_input_menu: represents a 'pok_message_menu' that has text input capabilities */
struct pok_input_menu
{
    struct pok_menu base;
    struct pok_text_input input;  /* menu input object */
};
void pok_input_menu_init(struct pok_input_menu* menu,const struct pok_graphics_subsystem* sys);
void pok_input_menu_delete(struct pok_input_menu* menu);
void pok_input_menu_ctrl_key(struct pok_input_menu* menu,enum pok_input_key key);
void pok_input_menu_activate(struct pok_input_menu* menu,const char* prompt);
void pok_input_menu_deactivate(struct pok_input_menu* menu);
void pok_input_menu_render(struct pok_input_menu* menu);

/* pok_selection_menu: represents a menu where each text line is a selection
   choice; the menu automatically sizes according to the longest text string
   item; the user gets to choose the menu's location on the screen */
struct pok_selection_menu
{
    struct pok_menu base;
    struct pok_text_context text;
    uint32_t clipWidth;
    uint32_t clipHeight;
    int32_t sel;
};
void pok_selection_menu_init(struct pok_selection_menu* menu,uint32_t ndisplay,
    uint32_t clipWidth,const struct pok_graphics_subsystem* sys);
void pok_selection_menu_delete(struct pok_selection_menu* menu);
void pok_selection_menu_float(struct pok_selection_menu* menu,int vfloat);
void pok_selection_menu_add_item(struct pok_selection_menu* menu,const char* item);
void pok_selection_menu_add_list(struct pok_selection_menu* menu,const char* items[]); /* NULL-terminated list */
void pok_selection_menu_activate(struct pok_selection_menu* menu);
void pok_selection_menu_deactivate(struct pok_selection_menu* menu);
void pok_selection_menu_ctrl_key(struct pok_selection_menu* menu,enum pok_input_key key);
void pok_selection_menu_render(struct pok_selection_menu* menu);

/* pok_yesno_menu: a subclass of 'pok_selection_menu' that is automatically
   configured to contain 'Yes' and 'No' selections; the 'pok_selection_menu_*'
   functions should be called on it (except '*_init()') */
void pok_yesno_menu_init(struct pok_selection_menu* menu,const struct pok_graphics_subsystem* sys);

#endif
