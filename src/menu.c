/* menu.c - pokgame */
#include "menu.h"
#include "error.h"
#include "config.h"
#include "opengl.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* default parameters */
#define DEFAULT_TEXT_UPDATE_TICKS 30

/* glyph functionality */
#define POK_GLYPH_COUNT 94
#define POK_GLYPH_START 33
#define POK_GLYPH_END   126
#define POK_GLYPH_WIDTH 16
#define POK_GLYPH_HEIGHT 32
#define FROM_ASCII(c) c >= POK_GLYPH_START && c <= POK_GLYPH_END ? c - POK_GLYPH_START : -1

/* we read glyphs as OpenGL textures from a source image in the install directory */
static bool_t glyphsLoaded = FALSE;
static struct pok_image glyphImage = { 0, POK_GLYPH_WIDTH, POK_GLYPH_HEIGHT, {NULL}, 0, {{0,0,0}} };
static GLuint glyphTextures[POK_GLYPH_COUNT];

void pok_glyphs_load()
{
    /* this routines loads glyphs from file as OpenGL textures; it must be called from the
       graphics thread to which the OpenGL context is bound */
    size_t i;
    GLenum format;
    size_t channels;
    struct pok_image* glyphSource;
    struct pok_string* path;

    /* read the glyphs into memory from file */
    path = pok_get_install_root_path();
    pok_string_concat(path,POKGAME_INSTALL_GLYPHS_FILE);
    glyphSource = pok_image_png_new(path->buf);
    pok_string_free(path);
    if (glyphSource == NULL)
        pok_error_fromstack(pok_error_fatal);
    if (glyphSource->width != POK_GLYPH_WIDTH || glyphSource->height != POK_GLYPH_HEIGHT * POK_GLYPH_COUNT)
        pok_error(pok_error_fatal,"glyph image had incorrect dimensions");
    format = glyphSource->flags & pok_image_flag_alpha ? GL_RGBA : GL_RGB;
    channels = glyphSource->flags & pok_image_flag_alpha ? 4 : 3;

    /* load the glyphs as 2D textures into the GL */
    glGenTextures(POK_GLYPH_COUNT,glyphTextures);
    for (i = 0;i < POK_GLYPH_COUNT;++i) {
        glBindTexture(GL_TEXTURE_2D,glyphTextures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,TEXPARAM);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,TEXPARAM);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,GL_RGBA,
            POK_GLYPH_WIDTH,
            POK_GLYPH_HEIGHT,
            0,
            format,
            GL_UNSIGNED_BYTE,
            (byte_t*)glyphSource->pixels.data + i * POK_GLYPH_WIDTH * POK_GLYPH_HEIGHT * channels );
    }
    glyphsLoaded = TRUE;
    pok_image_free(glyphSource);
}
void pok_glyphs_unload()
{
    /* unload glyph textures: this routine must be called on the graphics thread */
    glDeleteTextures(POK_GLYPH_COUNT,glyphTextures);
    glyphsLoaded = FALSE;
}
struct pok_image* pok_glyph(int c)
{
    /* if some other functionality needs to access a glyph, then we return a dummy
       image object with a texture reference to the glyph */
    c = FROM_ASCII(c);
    if (c == -1)
        /* no glyph matches character */
        return NULL;
    glyphImage.texref = glyphTextures[c];
    return &glyphImage;
}
static void pok_glyph_render(int c,int32_t x,int32_t y)
{
    int32_t X, Y;
    c = FROM_ASCII(c);
    if (c == -1)
        /* character has no associated glyph */
        return;
    X = x + POK_GLYPH_WIDTH;
    Y = y + POK_GLYPH_HEIGHT;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,glyphTextures[c]);
    glBegin(GL_QUADS);
    {
        glTexCoord2i(0,0);
        glVertex2i(x,y);
        glTexCoord2i(1,0);
        glVertex2i(X,y);
        glTexCoord2i(1,1);
        glVertex2i(X,Y);
        glTexCoord2i(0,1);
        glVertex2i(x,Y);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

struct text_position
{
    int32_t line;
    int32_t pos;
};

/* pok_text_context */
void pok_text_context_init(struct pok_text_context* text)
{
    text->x = 0;
    text->y = 0;
    text->region = (struct pok_size){ 0, 0 };
    text->finished = TRUE;
    text->curline = 0;
    text->curcount = 0;
    text->progress = 0;
    text->lines = NULL;
    text->linecnt = 0;
    text->linealloc = 0;
    text->updateTicks = 0;
    text->updateTicksAmt = DEFAULT_TEXT_UPDATE_TICKS;
}
void pok_text_context_delete(struct pok_text_context* text)
{
    size_t i;
    for (i = 0;i < text->linealloc;++i)
        free(text->lines[i]);
    free(text->lines);
}
static bool_t allocate_lines(struct pok_text_context* text)
{
    if (text->linecnt >= text->linealloc) {
        char** lines;
        size_t alloc = text->linealloc;
        do {
            /* keep doubling the allocation until it is high enough */
            alloc = alloc == 0 ? 8 : (alloc << 1);
        } while (alloc < text->linecnt);
        lines = realloc(text->lines,sizeof(char*) * alloc);
        if (lines == NULL) {
            pok_error(pok_error_warning,"failed memory allocation for pok_text_context");
            return FALSE;
        }
        text->lines = lines;
        for (;text->linealloc < alloc;++text->linealloc) {
            lines[text->linealloc] = malloc(text->region.columns + 1);
            if (lines[text->linealloc] == NULL) {
                pok_error(pok_error_warning,"failed memory allocation for pok_text_context");
                return FALSE;
            }
            lines[text->linealloc][0] = 0; /* terminate string */
        }
    }
    return TRUE;
}
static void insert_text_recursive(struct pok_text_context* text,const char* message,struct text_position* startPos)
{
    uint32_t i;
    char* linebuf;
    uint32_t length;

    /* grab line buffer; offset to specified position */
    if (startPos->line >= (int32_t)text->linecnt) {
        text->linecnt = startPos->line+1;
        allocate_lines(text);
    }
    linebuf = text->lines[startPos->line] + startPos->pos;

    /* compute length of message substring that we can append to this line */
    i = startPos->pos;
    length = 0;
    while (i < text->region.columns && message[length] && message[length] != '\n')
        ++i, ++length;

    if (message[length] == '\n')
        ++length;

    /* insert the text (if there is anything to insert) */
    if (length > 0) {
        size_t displacedLength;
        char* displaced;
        struct text_position posCpy;

        /* copy displaced text (if any) to a temporary buffer */
        displacedLength = strlen(linebuf);
        if (displacedLength == 0)
            displaced = NULL;
        else {
            /* save displaced line contents; we'll recursively insert them later */
            displaced = malloc(text->region.columns + 1);
            if (displaced == NULL) {
                pok_error(pok_error_warning,"failed memory allocation for pok_text_context");
                return;
            }
            strcpy(displaced,linebuf);
        }

        /* copy message (the portion which we can fit on the line) into buffer */
        strncpy(linebuf,message,length);
        linebuf[length] = 0;

        /* compute position for potential recursive insertion */
        if (startPos->pos + length >= text->region.columns) {
            /* the insertion filled up the linebuffer so update the position to the beginning of the next line */
            startPos->pos = 0;
            ++startPos->line;
        }
        else
            startPos->pos += length;

        /* recursively insert any remaining characters */
        if (message[length])
            insert_text_recursive(text,message + length,startPos);

        /* recursively insert any displaced characters; create a copy of the text position
           info so that we don't modify the original (this preserves correct ordering) */
        posCpy = *startPos;
        if (displaced != NULL) {
            insert_text_recursive(text,displaced,&posCpy);
            free(displaced);
        }
    }

    /* else, base case: no recursive insertions */
}
static void find_word(struct pok_text_context* text,struct text_position* start,struct text_position* end)
{
    /* adjust 'start' and 'end' until they refer to a complete word; this function assumes that they point
       to non-space characters and that 'start' <= 'end' */
    int32_t l;
    /* find start of edited word */
    while (start->line > 0 || start->pos > 0) {
        if ( isspace(text->lines[start->line][start->pos]) )
            break;
        if (start->pos == 0) {
            --start->line;
            start->pos = strlen(text->lines[start->line]) - 1;
        }
        else
            --start->pos;
    }
    /* find end of edited word */
    l = strlen(text->lines[end->line])-1;
    while (end->line < (int32_t)text->linecnt-1 || end->pos < l) {
        if ( isspace(text->lines[end->line][end->pos]) )
            break;
        if (end->pos == l) {
            ++end->line;
            end->pos = 0;
            l = strlen(text->lines[end->line])-1;
        }
        else
            ++end->pos;
    }
    /* make position inclusive to the word */
    if (isspace(text->lines[start->line][start->pos]) /*|| !text->lines[start->line][start->pos]*/) {
        if (start->pos == (int32_t)strlen(text->lines[start->line]) - 1) {
            if (start->line+1 < (int32_t)text->linecnt) {
                start->pos = 0;
                ++start->line;
            }
        }
        else
            ++start->pos;
    }
    if (isspace(text->lines[end->line][end->pos]) || !text->lines[end->line][end->pos]) {
        if (end->pos == 0) {
            if (end->line > 0) {
                --end->line;
                end->pos = strlen(text->lines[end->line])-1;
            }
        }
        else
            --end->pos;
    }
}
static int32_t check_word_wrap(struct pok_text_context* text,const struct text_position* start,const struct text_position* end)
{
    /* check to see if word should/can be wrapped so that it appears on the same line; a word may only be wrapped
       if it is not longer than the longest line length for the text context; return the number of characters
       wrapped to the next line or zero if no characters wrapped */
    int32_t l;
    l = strlen(text->lines[start->line] + start->pos);
    if (end->line - start->line == 1 && l + end->pos + 1 <= text->region.columns) {
        /* we can wrap the start line (from 'start->pos' to end of line) to the next line; we
           do this by displacing the text from the lesser-ordered line to the higher-ordered
           line; this may recursively displace (and potentially wrap) other text */
        int32_t r;
        char* displacement = malloc(l+1);
        struct text_position pos = *end;
        if (displacement == NULL) {
            pok_error(pok_error_warning,"failed memory allocation for pok_text_context");
            return 0;
        }
        strcpy(displacement,text->lines[start->line] + start->pos);
        pos.pos = 0;
        text->lines[start->line][start->pos] = 0;
        insert_text_recursive(text,displacement,&pos);
        r = strlen(displacement);
        free(displacement);
        return r;
    }
    return 0;
}
static void check_wrap(struct pok_text_context* text,struct text_position* adjustPos,struct text_position* startPos)
{
    struct text_position start, end;
    start.line = end.line = startPos->line;
    start.pos = end.pos = 0;
    if ( !isspace(text->lines[start.line][start.pos]) ) {
        int32_t r;
        find_word(text,&start,&end);
        r = check_word_wrap(text,&start,&end);
        if (r) {
            /* if a word was wrapped, check to see if the wrap effected the adjust position and update accordingly */
            if (adjustPos->line == start.line && adjustPos->pos >= start.pos) {
                adjustPos->line = end.line;
                adjustPos->pos = adjustPos->pos - start.pos;
            }
            else if (adjustPos->line == end.line)
                adjustPos->pos += r;
        }
    }
}
static void post_edit(struct pok_text_context* text,struct text_position* startPos)
{
    struct text_position dummy;
    /* check wrap on current line; if a wrap occurs, then this will update the 'startPos->pos' so that
       it still refers to the same relative position within the text */
    check_wrap(text,startPos,startPos);
    /* now check the rest of the lines */
    for (dummy.pos = 0, dummy.line = startPos->line+1;dummy.line < (int32_t)text->linecnt;++dummy.line)
        check_wrap(text,startPos,&dummy);
}
static void insert_text(struct pok_text_context* text,const char* message,struct text_position* startPos)
{
    /* insert a text string and then perform the post edit operation to format the text */
    insert_text_recursive(text,message,startPos);
    post_edit(text,startPos);
}

static struct text_position remove_text(struct pok_text_context* text,struct text_position startPos,struct text_position endPos)
{
    char* linebuf;
    size_t length;
    struct text_position cpy;
    ++endPos.pos; /* we want to refer to the next character (or end of string) in the buffer */

    /* assign a null byte to the beginning of every line within the delete region */
    cpy = startPos;
    do {
        if (cpy.pos == 0)
            text->lines[cpy.line][cpy.pos] = 0;
        ++cpy.line;
        cpy.pos = 0;
    } while (cpy.line <= endPos.line);
    cpy = startPos;

    /* go through the lines after the delete region and insert their contents over the
       text to delete; this effectively shifts the text context's contents up */
    while (endPos.line < (int32_t)text->linecnt) {
        /* grab pointer to line buffer */
        linebuf = text->lines[endPos.line] + endPos.pos;
        if (*linebuf) {
            /* copy line and terminate it (clear) */
            char* linecpy;
            length = strlen(linebuf) + 1;
            linecpy = malloc(length);
            strcpy(linecpy,linebuf);
            memset(linebuf,0,length);

            /* insert line text into start position */
            text->lines[startPos.line][startPos.pos] = 0;
            insert_text_recursive(text,linecpy,&startPos);
            free(linecpy);
        }

        /* update end position to next line */
        endPos.pos = 0;
        ++endPos.line;
    }

    /* potentially decrease the line allocation; assign a null-byte to the start
       of each line (make each deallocated line empty) */
    while ((int32_t)text->linecnt > startPos.line+1)
        text->lines[--text->linecnt][0] = 0;

    /* perform a post edit to reformat the text */
    post_edit(text,&cpy);
    return cpy;
}
void pok_text_context_assign(struct pok_text_context* text,const char* message,const struct pok_size* region)
{
    size_t i;
    struct text_position pos;
    /* deallocate any allocated lines by assigning a null byte to the beginning */
    for (;text->linecnt > 0;--text->linecnt)
        text->lines[text->linecnt-1][0] = 0;
    text->region = *region;
    pos.line = 0;
    pos.pos = 0;
    insert_text(text,message,&pos);
    /* setup contextual information */
    text->finished = FALSE;
    text->curline = 0;
    text->curcount = 0;
    for (i = 0;i < text->region.rows && i < text->linecnt;++i)
        text->curcount += strlen(text->lines[i]);
    text->progress = 0;
}
bool_t pok_text_context_update(struct pok_text_context* text,uint32_t ticks)
{
    /* update the context so that the text appears to scroll across the screen; we only perform an
       update after the specified amount of time has elapsed; the function returns non-zero if the
       update sequence is still animating, zero otherwise */
    if (!text->finished && text->progress < text->curcount) {
        uint32_t times;
        text->updateTicks += ticks;
        times = text->updateTicks / text->updateTicksAmt;
        text->updateTicks %= text->updateTicksAmt;
        if (times > 0)
            text->progress += times;
        return TRUE;
    }
    return FALSE;
}
bool_t pok_text_context_next(struct pok_text_context* text)
{
    /* displays the next sequence of lines; returns false if there are no more lines
       to display on the next call to this function (in other words if the context has
       not finished), true otherwise */
    if (!text->finished) {
        uint32_t i, r;
        text->curline += text->region.rows;
        text->curcount = 0;
        for (i = 0, r = text->curline;i < text->region.rows && r < text->linecnt;++i, ++r)
            text->curcount += strlen(text->lines[r]);
        text->progress = 0;
        /* we want to return true if THE NEXT call will still have text to show */
        text->finished = text->linecnt - text->curline <= text->region.rows;
        return !text->finished;
    }
    return FALSE;
}
void pok_text_context_render(struct pok_graphics_subsystem* sys,struct pok_text_context* text)
{
    uint32_t i;
    uint32_t line;
    uint32_t c;
    uint32_t y;

    /* render just the viewable lines of text (defined by text region) */
    c = 0;
    for (i = 0, line = text->curline, y = text->y;i < text->region.rows && line < text->linecnt;++i, ++line, y += POK_GLYPH_HEIGHT) {
        uint32_t x = text->x;
        const char* linebuf = text->lines[line];
        /* render a line of text: if we have rendered up to 'progress' number of characters then stop */
        while (*linebuf && (c < text->progress || text->finished)) {
            /* render glyph */
            pok_glyph_render(*linebuf,x,y);
            ++linebuf, ++c, x += POK_GLYPH_WIDTH;
        }
    }

    (void)sys;
}

/* pok_text_input */
void pok_text_input_init(struct pok_text_input* ti)
{
    pok_text_context_init(&ti->base);
    ti->line = ti->iniLine = 0;
    ti->pos = ti->iniPos = 0;
    ti->finished = TRUE;
}
void pok_text_input_delete(struct pok_text_input* ti)
{
    pok_text_context_delete(&ti->base);
}
static void update_render_position_bypage(struct pok_text_input* ti)
{
    /* update render position within text context; this only needs to happen when
       the current edit line has changed; this update method shows a page of text
       at a time and jumps between pages (as opposed to scrolling) */
    int32_t dif;
    dif = ti->line - ti->base.curline;
    if (ti->line < ti->base.curline || dif >= ti->base.region.rows) {
        /* find the line that will display the current edit line; we solve this problem by rounding the edit line
           down to a multiple of the number of viewable lines allowed in the text region; we must compute a modulus
           that always has a positive sign by flooring the quotient; we subtract this remainder from the edit line
           to find the new display line */
        ti->base.curline = ti->line - (dif - ((dif / ti->base.region.rows) - (dif < 0)) * ti->base.region.rows);
    }
}
static bool_t move_cursor_up(struct pok_text_input* ti)
{
    /* move the position selection up a line */
    int32_t tryPos, tryLine;
    int32_t len;
    tryPos = ti->pos;
    tryLine = ti->line - 1;
    if (tryLine < ti->iniLine)
        return FALSE;
    if (tryLine == ti->iniLine) {
        if (tryPos < ti->iniPos)
            tryPos = ti->iniPos;
        else {
            len = strlen(ti->base.lines[tryLine]) - 1;
            if (tryPos > len)
                tryPos = len;
        }
    }
    ti->line = tryLine;
    ti->pos = tryPos;
    update_render_position_bypage(ti);
    return TRUE;
}
static bool_t move_cursor_down(struct pok_text_input* ti)
{
    /* move the position selection down a line */
    int32_t tryPos, tryLine;
    int32_t len;
    tryPos = ti->pos;
    tryLine = ti->line + 1;
    if (tryLine >= (int32_t)ti->base.linecnt)
        return FALSE;
    len = strlen(ti->base.lines[tryLine]) - 1;
    if (tryPos > len)
        tryPos = len;
    ti->line = tryLine;
    ti->pos = tryPos;
    update_render_position_bypage(ti);
    return TRUE;
}
static bool_t move_cursor_left(struct pok_text_input* ti)
{
    /* move the position left (wrap to line above if out of range) */
    int32_t tryPos, tryLine;
    tryPos = ti->pos - 1;
    tryLine = ti->line;
    if (tryPos < 0) {
        --tryLine;
        if (tryLine < ti->iniLine)
            return FALSE;
        tryPos = strlen(ti->base.lines[tryLine])-1;
    }
    if (tryLine == ti->iniLine && tryPos < ti->iniPos)
        return FALSE;
    ti->line = tryLine;
    ti->pos = tryPos;
    update_render_position_bypage(ti);
    return TRUE;
}
static bool_t move_cursor_right(struct pok_text_input* ti)
{
    /* move the position right (wrap to line below if out of range) */
    int32_t tryPos, tryLine;
    tryPos = ti->pos + 1;
    tryLine = ti->line;
    if (tryPos >= (int32_t)strlen(ti->base.lines[tryLine])) {
        ++tryLine;
        if (tryLine >= (int32_t)ti->base.linecnt)
            return FALSE;
        tryPos = 0;
    }
    ti->line = tryLine;
    ti->pos = tryPos;
    update_render_position_bypage(ti);
    return TRUE;
}
void pok_text_input_assign(struct pok_text_input* ti,const char* prompt,const struct pok_size* region)
{
    struct text_position pos;
    /* assign prompt as initial text in text context */
    pok_text_context_assign(&ti->base,prompt,region);
    pos.line = ti->base.linecnt - 1;
    pos.pos = strlen(ti->base.lines[ti->line]);
    ti->finished = FALSE;
    /* insert two space characters:
        the first makes sure that the prompt is separated from the user text
        the second makes sure that the insert position is valid at all times
    */
    insert_text(&ti->base,"  ",&pos);
    /* compute initial position (one less than position after insert) and assign
       it as the initial cursor position */
    ti->iniPos = pos.pos - 1;
    ti->iniLine = pos.line;
    if (ti->iniPos < 0) {
        --ti->iniLine;
        ti->iniPos = strlen(ti->base.lines[ti->iniLine])-1;
    }
    ti->line = ti->iniLine;
    ti->pos = ti->iniPos;
}
void pok_text_input_reset(struct pok_text_input* ti)
{
    if (ti->base.linecnt > 0) {
        /* clear everything past the initial position */
        ti->base.lines[ti->iniLine][ti->iniPos] = ' ';
        ti->base.lines[ti->iniLine][ti->iniPos+1] = 0;
        for (;ti->base.linecnt > (uint32_t)ti->iniLine+1;--ti->base.linecnt)
            ti->base.lines[ti->base.linecnt-1][0] = 0;
        ti->pos = ti->iniPos;
        ti->line = ti->iniLine;
        update_render_position_bypage(ti);
        ti->finished = FALSE;
    }
}
/* Note: the below functions for 'pok_text_input' should be executed on the rendering thread
   which means there cannot be a race condition between them and the text context rendering */
static void protect_inipos(struct pok_text_input* ti)
{
    char* p;
    /* make sure that initial position is protected by space character */
    p = ti->base.lines[ti->iniLine] + ti->iniPos;
    if (*p == 0) {
        *p = ' ';
        *(p+1) = 0;
    }
}
void pok_text_input_entry(struct pok_text_input* ti,char c)
{
    /* only accept input once the text context has finished displaying the prompt */
    if (ti->base.finished && !ti->finished) {
        char message[2] = { c, 0 };
        struct text_position pos = { ti->line, ti->pos };

        /* insert text and update cursor/render position */
        insert_text(&ti->base,message,&pos);
        ti->pos = pos.pos;
        ti->line = pos.line;
        update_render_position_bypage(ti);
        protect_inipos(ti);
    }
}
bool_t pok_text_input_ctrl_key(struct pok_text_input* ti,enum pok_input_key key)
{
    /* returns non-zero if the input object has finished gathering input */
    if (ti->base.finished && !ti->finished) {
        if (key == pok_input_key_ENTER)
            /* when the user types enter they complete the input sequence */
            ti->finished = TRUE;
        else if (key == pok_input_key_BACK) {
            /* delete character to the left */
            if ( move_cursor_left(ti) ) {
                remove_text(&ti->base,(struct text_position){ti->line,ti->pos},(struct text_position){ti->line,ti->pos});
                update_render_position_bypage(ti);
            }
        }
        else if (key == pok_input_key_DEL) {
            /* delete currently selected character */
            struct text_position newpos;
            if (ti->line < (int32_t)ti->base.linecnt-1 || ti->pos < (int32_t)strlen(ti->base.lines[ti->line])-1)
                newpos = remove_text(&ti->base,(struct text_position){ti->line,ti->pos},(struct text_position){ti->line,ti->pos});
            else
                return ti->finished;
            ti->pos = newpos.pos;
            ti->line = newpos.line;
            protect_inipos(ti);
            update_render_position_bypage(ti);
        }
        else if (key == pok_input_key_UP)
            move_cursor_up(ti);
        else if (key == pok_input_key_DOWN)
            move_cursor_down(ti);
        else if (key == pok_input_key_LEFT)
            move_cursor_left(ti);
        else if (key == pok_input_key_RIGHT)
            move_cursor_right(ti);
    }
    return ti->finished;
}
void pok_text_input_read(struct pok_text_input* ti,struct pok_string* buffer)
{
    uint32_t i;
    const char* lbuf;
    i = ti->iniLine;
    lbuf = ti->base.lines[i] + ti->iniPos;
    /* skip over leading whitespace */
    while ( isspace(*lbuf) )
        ++lbuf;
    while (TRUE) {
        pok_string_concat(buffer,lbuf);
        if (++i >= ti->base.linecnt)
            break;
        lbuf = ti->base.lines[i];
    }
    /* decrease string length to trim off trailing whitespace */
    if (buffer->len > 0) {
        while ( isspace(buffer->buf[buffer->len-1]) )
            --buffer->len;
        buffer->buf[buffer->len] = 0;
    }
}
void pok_text_input_render(struct pok_graphics_subsystem* sys,struct pok_text_input* ti)
{
    int32_t x, X, y, Y;

    /* draw the cursor (a box that inverts the pixels it bounds) */
    x = ti->base.x + ti->pos * POK_GLYPH_WIDTH;
    y = ti->base.y + (ti->line % ti->base.region.rows) * POK_GLYPH_HEIGHT;
    X = x + POK_GLYPH_WIDTH;
    Y = y + POK_GLYPH_HEIGHT;
    glColor3b(20,20,20);
    glBegin(GL_QUADS);
    {
        glVertex2i(x,y);
        glVertex2i(X,y);
        glVertex2i(X,Y);
        glVertex2i(x,Y);
    }
    glEnd();

    /* call base class render function */
    pok_text_context_render(sys,&ti->base);
}
