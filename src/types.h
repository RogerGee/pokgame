/* types.h - pokgame */
#ifndef POKGAME_TYPES_H
#define POKGAME_TYPES_H
#include <stddef.h>
#include <stdint.h>

typedef unsigned char bool_t;
typedef unsigned char byte_t;

#define TRUE 1
#define FALSE 0

struct pok_string
{
    char* buf;
    size_t len, cap;
};
struct pok_string* pok_string_new();
struct pok_string* pok_string_new_ex(size_t initialCapacity);
void pok_string_init(struct pok_string* str);
void pok_string_init_ex(struct pok_string* str,size_t initialCapacity);
void pok_string_delete(struct pok_string* str);
void pok_string_assign(struct pok_string* str,const char* s);
void pok_string_copy(struct pok_string* str,const struct pok_string* operand);
void pok_string_concat(struct pok_string* str,const char* s);
void pok_string_concat_ex(struct pok_string* str,const char* s,size_t length);
void pok_string_concat_obj(struct pok_string* str,const struct pok_string* operand);
void pok_string_concat_char(struct pok_string* str,char c);
void pok_string_clear(struct pok_string* str);
void pok_string_reset(struct pok_string* str);

#endif
