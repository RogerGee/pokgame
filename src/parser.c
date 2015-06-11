/* parser.c - pokgame */
#include "parser.h"
#include "error.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* pok_parser_info */
struct pok_parser_info* pok_parser_info_new()
{
    struct pok_parser_info* info = malloc(sizeof(struct pok_parser_info));
    if (info == NULL) {
        pok_exception_flag_memory_error();
        return NULL;
    }
    pok_parser_info_init(info);
    return info;
}
void pok_parser_info_free(struct pok_parser_info* info)
{
    pok_parser_info_delete(info);
    free(info);
}
void pok_parser_info_init(struct pok_parser_info* info)
{
    int i;
    info->dsrc = NULL;
    info->lineno = 1;
    info->bytes = NULL;
    info->words = NULL;
    info->qwords = NULL;
    info->strings = NULL;
    for (i = 0;i < 2;++i) {
        info->bytes_c[i] = 0;
        info->words_c[i] = 0;
        info->qwords_c[i] = 0;
        info->strings_c[i] = 0;
    }
    info->separator = ' ';
}
void pok_parser_info_delete(struct pok_parser_info* info)
{
    size_t i;
    free(info->bytes);
    free(info->words);
    free(info->qwords);
    for (i = 0;i < info->strings_c[0];++i)
        pok_string_free(info->strings[i]);
    free(info->strings);
}
void pok_parser_info_reset(struct pok_parser_info* info)
{
    int i;
    for (i = 0;i < 2;++i) {
        info->bytes_c[i] = 0;
        info->words_c[i] = 0;
        info->qwords_c[i] = 0;
        info->strings_c[i] = 0;
    }
}
static bool_t pok_parser_info_check_generic(void** array,size_t elemsize,size_t info[])
{
    if (*array == NULL) {
        info[1] = 8;
        *array = malloc(elemsize * info[1]);
        if (*array == NULL) {
            pok_exception_flag_memory_error();
            return FALSE;
        }
    }
    else if (info[0] >= info[1]) {
        void* newarray;
        size_t newsize;
        newsize = info[1] << 1;
        newarray = realloc(*array,elemsize * newsize);
        if (newarray == NULL) {
            pok_exception_flag_memory_error();
            return FALSE;
        }
        *array = newarray;
        info[1] = newsize;
    }
    return TRUE;
}
static inline bool_t pok_parser_info_check_bytes(struct pok_parser_info* info)
{ return pok_parser_info_check_generic((void**)&info->bytes,1,info->bytes_c); }
static inline bool_t pok_parser_info_check_words(struct pok_parser_info* info)
{ return pok_parser_info_check_generic((void**)&info->words,sizeof(uint16_t),info->words_c); }
static inline bool_t pok_parser_info_check_qwords(struct pok_parser_info* info)
{ return pok_parser_info_check_generic((void**)&info->qwords,sizeof(uint32_t),info->qwords_c); }
static inline bool_t pok_parser_info_check_strings(struct pok_parser_info* info)
{ return pok_parser_info_check_generic((void**)&info->strings,sizeof(struct pok_string*),info->strings_c); }

/* grammar/primatives
    [number]: '-'? [0-9]+

    [string]: ( [a-z] | [A-Z] | [0-9] | '_' )+

    [newline]: ( '\n' | '\r' )+

    [sep]: ( ' ' | '\t' )* info->sep ( ' ' | '\t' )*

    [wspace]: ( ' ' | '\t' | '\r' | '\n' )+

    [wspace-opt]: ( ' ' | '\t' | '\r' | '\n' )*

    [keyword]: token ( [a-z] | [A-Z] )+ [0-9]*  matches given keyword (case sensitivity can be ignored)

    <coord>:
     '{' [wspace-opt] [number] [wspace-opt] ',' [wspace-opt] [number] [wspace-opt] '}'

    anything else is grammar-specific
 */
static bool_t pok_parse_number(struct pok_parser_info* info)
{
    char c;
    int i, j;
    bool_t neg;
    char buf[128];
    i = 0;
    c = pok_data_source_peek(info->dsrc);
    if (c == '-') {
        neg = TRUE;
        pok_data_source_pop(info->dsrc);
        c = pok_data_source_peek(info->dsrc);
    }
    else
        neg = FALSE;
    if (c >= '0' && c <= '9') {
        buf[i++] = c;
        pok_data_source_pop(info->dsrc);
    }
    else
        return FALSE;
    while (TRUE) {
        c = pok_data_source_peek(info->dsrc);
        if (c >= '0' && c <= '9') {
            buf[i++] = c;
            pok_data_source_pop(info->dsrc);
        }
        else
            break;
    }
    info->number = 0;
    for (j = 0;j < i;++j) {
        info->number *= 10;
        info->number += buf[j] - '0';
    }
    if (neg)
        info->number = -info->number;
    return TRUE;
}

static bool_t pok_parse_byte(struct pok_parser_info* info)
{
    if (pok_parse_number(info) && pok_parser_info_check_bytes(info)) {
        info->bytes[info->bytes_c[0]++] = (uint8_t) info->number;
        return TRUE;
    }
    return FALSE;
}

static bool_t pok_parse_word(struct pok_parser_info* info)
{
    if (pok_parse_number(info) && pok_parser_info_check_words(info)) {
        info->words[info->words_c[0]++] = (uint16_t) info->number;
        return TRUE;
    }
    return FALSE;
}

static bool_t pok_parse_qword(struct pok_parser_info* info)
{
    if (pok_parse_number(info) && pok_parser_info_check_qwords(info)) {
        info->qwords[info->qwords_c[0]++] = (uint32_t) info->number;
        return TRUE;
    }
    return FALSE;
}

static bool_t pok_parse_string(struct pok_parser_info* info)
{
    char c;
    struct pok_string* string;\
    string = pok_string_new();
    c = pok_data_source_peek(info->dsrc);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
        pok_string_concat_char(string,c);
    else {
        pok_string_free(string);
        return FALSE;
    }
    pok_data_source_pop(info->dsrc);
    while (TRUE) {
        c = pok_data_source_peek(info->dsrc);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
            pok_string_concat_char(string,c);
            pok_data_source_pop(info->dsrc);
        }
        else
            break;        
    }
    if ( pok_parser_info_check_strings(info) ) {
        info->strings[info->strings_c[0]++] = string;
        return TRUE;
    }
    pok_string_free(string);
    return FALSE;
}

static bool_t pok_parse_newline(struct pok_parser_info* info)
{
    /* nothing is extracted here */
    char c;
    c = pok_data_source_peek(info->dsrc);
    if (c != '\n' && c != '\r')
        return FALSE;
    do {
        pok_data_source_pop(info->dsrc);
        c = pok_data_source_peek(info->dsrc);
    } while (c == '\n' || c == '\r');
    ++info->lineno;
    return TRUE;
}

static bool_t pok_parse_sep(struct pok_parser_info* info)
{
    /* nothing is extracted here */
    char c;
    while (TRUE) {
        c = pok_data_source_peek(info->dsrc);
        if (c != ' ' && c != '\t')
            break;
        pok_data_source_pop(info->dsrc);
    }
    if (c != info->separator)
        return FALSE;
    do {
        pok_data_source_pop(info->dsrc);
        c = pok_data_source_peek(info->dsrc);
    } while (c == ' ' || c == '\t');
    return TRUE;
}

static bool_t pok_parse_wspace(struct pok_parser_info* info)
{
    /* nothing is extracted here */
    char c;
    bool_t found = FALSE;
    while (TRUE) {
        c = pok_data_source_peek(info->dsrc);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            break;
        if (c == '\n' || c == '\r')
            ++info->lineno;
        pok_data_source_pop(info->dsrc);
        found = TRUE;
    }
    return found;
}

static bool_t pok_parse_wspace_optional(struct pok_parser_info* info)
{
    /* nothing is extracted here */
    char c;
    while (TRUE) {
        c = pok_data_source_peek(info->dsrc);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            return TRUE;
        if (c == '\n' || c == '\r')
            ++info->lineno;
        pok_data_source_pop(info->dsrc);
    }
    return TRUE;
}

static bool_t pok_parse_keyword(struct pok_parser_info* info,const char* keywrd,bool_t caseSensitive)
{
    /* nothing is extracted here; just verify the keyword matches */
    char c;
    size_t depth = 0, br;
    while ( keywrd[depth] ) {
        c = pok_data_source_peek_ex(info->dsrc,depth);
        if (keywrd[depth] != c && (caseSensitive || tolower(c) != tolower(keywrd[depth])))
            return FALSE;
        ++depth;
    }
    /* pop bytes */
    pok_data_source_read(info->dsrc,depth,&br);
    return TRUE;
}

static bool_t pok_parse_coord(struct pok_parser_info* info)
{
    int i = 1;
    /* the two coordinates will be stored as qwords */
    if (pok_data_source_peek(info->dsrc) != '{')
        return FALSE;
    while (TRUE) {
        pok_data_source_pop(info->dsrc);
        pok_parse_wspace_optional(info);
        if ( !pok_parse_qword(info) )
            return FALSE;
        pok_parse_wspace_optional(info);
        if (i > 1)
            break;
        if (pok_data_source_peek(info->dsrc) != ',')
            return FALSE;
        ++i;
    }
    if (pok_data_source_peek(info->dsrc) != '}')
        return FALSE;
    pok_data_source_pop(info->dsrc);
    return TRUE;
}

/* grammar/pok-map/simple
    <file>:
      [number] [newline] [number] [newline] <row-data>

    <row-part>:
      [sep] <row>
      [newline]

    <row>:
      [number] <row-part>

    <row-data>:
      <row> <row-data>
      []      // empty

 */
static bool_t map_simple_row(struct pok_parser_info* info);
static bool_t map_simple_row_part(struct pok_parser_info* info)
{
    if ( pok_parse_sep(info) )
        return map_simple_row(info);
    if ( pok_parse_newline(info) )
        return TRUE;
    return FALSE;
}
bool_t map_simple_row(struct pok_parser_info* info)
{
    if ( pok_parse_word(info) )
        return map_simple_row_part(info);
    return FALSE;
}
static bool_t map_simple_row_data(struct pok_parser_info* info)
{
    if ( map_simple_row(info) )
        return map_simple_row_data(info);
    if (pok_data_source_peek(info->dsrc) == -1)
        return TRUE;
    pok_exception_new_ex2(info->lineno,"parse map simple: expected row or end of input");
    return FALSE;
}
bool_t pok_parse_map_simple(struct pok_parser_info* info)
{
    if ( !pok_parse_qword(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map simple: expected width");
        return FALSE;
    }
    if ( !pok_parse_newline(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map simple: expected newline after width");
        return FALSE;
    }
    if ( !pok_parse_qword(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map simple: expected height");
        return FALSE;
    }
    if ( !pok_parse_newline(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map simple: expected newline after height");
        return FALSE;
    }
    return map_simple_row_data(info);
}

/* grammar/pok-map/warps
    <file>:
     [wspace-opt] <warp-elems>

    <warp-elems>:
     <warp-elem> [wspace-opt] <warp-elems>
     []     // empty

    <warp-elem>:
     [keyword:"warp"] [wspace-opt] '(' [wspace-opt] [keyword:"map"] [wspace-opt] [number] [wspace-opt] [number] [wspace-opt] ')' [wspace-opt]
      '(' [wspace-opt] [keyword:"chunk"] [wspace-opt] <coord> [wspace-opt] <coord> [wspace-opt] ')' [wspace-opt] '(' [wspace-opt] [keyword:"pos"]
       [wspace-opt] <coord> [wspace-opt] <coord> [wspace-opt] ')' [wspace-opt] [string:warp kind label]

 */
static bool_t map_warps_warp_elem(struct pok_parser_info* info)
{
    struct pok_string* label;
    if ( !pok_parse_keyword(info,"warp",FALSE) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected 'warp' declaration");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    /* map element */
    if (pok_data_source_pop(info->dsrc) != '(') {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected '(' before 'map' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_keyword(info,"map",FALSE) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected 'map' declaration");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_qword(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected from-map-number in 'map' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_qword(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected to-map-number in 'map' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if (pok_data_source_pop(info->dsrc) != ')') {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected ')' after 'map' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    /* chunk element */
    if (pok_data_source_pop(info->dsrc) != '(') {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected '(' before 'chunk' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_keyword(info,"chunk",FALSE) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected 'chunk' declaration");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_coord(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected from-chunk-coordinate in 'chunk' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_coord(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected to-chunk-coordinate in 'chunk' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if (pok_data_source_pop(info->dsrc) != ')') {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected ')' after 'chunk' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    /* position element */
    if (pok_data_source_pop(info->dsrc) != '(') {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected '(' before 'pos' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_keyword(info,"pos",FALSE) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected 'pos' declaration");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_coord(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected from-position-coordinate in 'pos' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if ( !pok_parse_coord(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected to-position-coordinate in 'pos' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    if (pok_data_source_pop(info->dsrc) != ')') {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected ')' after 'pos' element");
        return FALSE;
    }
    pok_parse_wspace_optional(info);
    /* warp kind label */
    if ( !pok_parse_string(info) ) {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected warp kind label");
        return FALSE;
    }
    /* convert the label into the correct integer from protocol.h */
    label = info->strings[info->strings_c[0]-1];
    if ( !pok_parser_info_check_bytes(info) )
        return FALSE;
    if (strcmp(label->buf,"WARP_INSTANT") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_instant;
    else if (strcmp(label->buf,"WARP_LATENT_UP") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_latent_up;
    else if (strcmp(label->buf,"WARP_LATENT_DOWN") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_latent_down;
    else if (strcmp(label->buf,"WARP_LATENT_LEFT") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_latent_left;
    else if (strcmp(label->buf,"WARP_LATENT_RIGHT") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_latent_right;
    else if (strcmp(label->buf,"WARP_SPIN") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_spin;
    else if (strcmp(label->buf,"WARP_FALL") == 0)
        info->bytes[info->bytes_c[0]++] = pok_tile_warp_fall;
    else {
        pok_exception_new_ex2(info->lineno,"parse map warps: expected one of WARP_INSTANT, WARP_LATENT_UP, \
WARP_LATENT_DOWN, WARP_LATENT_LEFT, WARP_LATENT_RIGHT, WARP_SPIN or WARP_FALL for warp kind");
        return FALSE;
    }
    return TRUE;
}
static bool_t map_warps_warp_elems(struct pok_parser_info* info)
{
    if (pok_data_source_peek(info->dsrc) == -1)
        return TRUE; /* match empty statement */
    return map_warps_warp_elem(info)
        && pok_parse_wspace_optional(info)
        && map_warps_warp_elems(info);
}
bool_t pok_parse_map_warps(struct pok_parser_info* info)
{
    pok_parse_wspace_optional(info);
    return map_warps_warp_elems(info);
}
