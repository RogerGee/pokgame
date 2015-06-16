/* parser.h - pokgame */
#ifndef PARSER_H
#define PARSER_H
#include "net.h"

/* parsing: the parser module handles all non-binary parsing needed for the pokgame library */

struct pok_parser_info
{
    struct pok_data_source* dsrc;
    long long int number;
    int lineno;

    /* the parser extracts elements into these dynamic arrays */
    uint8_t* bytes;
    uint16_t* words;
    uint32_t* qwords;
    struct pok_string** strings;

    /* dynamic array lengths (index 0) and allocation limits (index 1) */
    size_t bytes_c[2];
    size_t words_c[2];
    size_t qwords_c[2];
    size_t strings_c[2];

    /* when specified by a grammar, the parser will use a variable separator character */
    char separator;
};
struct pok_parser_info* pok_parser_info_new();
void pok_parser_info_free(struct pok_parser_info* info);
void pok_parser_info_init(struct pok_parser_info* info);
void pok_parser_info_delete(struct pok_parser_info* info);
void pok_parser_info_reset(struct pok_parser_info* info);

/* cmdline **********************************************************************/
bool_t pok_parse_cmdline(struct pok_parser_info* info);
bool_t pok_parse_cmdline_ex(const char* cmdline,struct pok_string* buffer,const char*** argvOut);

/* pok_map **********************************************************************/
bool_t pok_parse_map_simple(struct pok_parser_info* info);
bool_t pok_parse_map_warps(struct pok_parser_info* info);

#endif
