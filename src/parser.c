/* parser.c - pokgame */
#include "parser.h"
#include "error.h"
#include <stdlib.h>

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

    [string]: ( [a-z] | [A-Z] | [0-9] )+

    [newline]: ( '\n' | '\r' )+

    [sep]: ( ' ' | '\t' )* info->sep ( ' ' | '\t' )*

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

static inline bool_t pok_parse_byte(struct pok_parser_info* info)
{
    if (pok_parse_number(info) && pok_parser_info_check_bytes(info)) {
        info->bytes[info->bytes_c[0]++] = (uint8_t) info->number;
        return TRUE;
    }
    return FALSE;
}

static inline bool_t pok_parse_word(struct pok_parser_info* info)
{
    if (pok_parse_number(info) && pok_parser_info_check_words(info)) {
        info->words[info->words_c[0]++] = (uint16_t) info->number;
        return TRUE;
    }
    return FALSE;
}

static inline bool_t pok_parse_qword(struct pok_parser_info* info)
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
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
        pok_string_concat_char(string,c);
    else {
        pok_string_free(string);
        return FALSE;
    }
    pok_data_source_pop(info->dsrc);
    while (TRUE) {
        c = pok_data_source_peek(info->dsrc);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
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

/* grammar/pok-map/simple
    <file>:
      [number] [newline] [number] [newline] <row-data>

    <row-part>:
      [sep] <row>
      [newline]    // empty

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
    pok_exception_new_ex2(info->lineno,"parse map simple: expected separator in row or end of row");
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
