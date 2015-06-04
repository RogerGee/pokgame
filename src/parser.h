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

    uint8_t* bytes;
    uint16_t* words;
    uint32_t* qwords;
    struct pok_string** strings;

    size_t bytes_c[2];
    size_t words_c[2];
    size_t qwords_c[2];
    size_t strings_c[2];

    char separator;
};
struct pok_parser_info* pok_parser_info_new();
void pok_parser_info_free(struct pok_parser_info* info);
void pok_parser_info_init(struct pok_parser_info* info);
void pok_parser_info_delete(struct pok_parser_info* info);
void pok_parser_info_reset(struct pok_parser_info* info);

/* pok_map **********************************************************************/
bool_t pok_parse_map_simple(struct pok_parser_info* info);

#endif
