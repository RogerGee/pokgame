/* types.h - pokgame */
#ifndef POKGAME_TYPES_H
#define POKGAME_TYPES_H
#include <stddef.h>
#include <stdint.h>

/* the Microsoft C Compiler does not support C99; therefore
   make every "inline" reference just an empty macro substitution */
#ifdef POKGAME_VISUAL_STUDIO
#define inline
#endif

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
void pok_string_free(struct pok_string* str);
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

/* define size by number of columns and rows */
struct pok_size
{
    uint16_t columns; /* width */
    uint16_t rows; /* height */
};

/* define location by column and row position:
    columns are numbered from [0..n] left-to-right; rows
    are numbered from [0..n] from top-to-bottom */
struct pok_location
{
    uint16_t column; /* x */
    uint16_t row; /* y */
};

#define pok_unsigned_diff(a,b) (a>b ? a-b : b-a)

struct pok_point
{
    int32_t X;
    int32_t Y;
};

/* define fundamental direction */
enum pok_direction
{
    pok_direction_up,
    pok_direction_down,
    pok_direction_left,
    pok_direction_right,
    pok_direction_none
};

#define pok_direction_opposite(dir) (dir==pok_direction_none ? (int)pok_direction_none : ((int)dir + ((int)dir%2==0 ? 1 : -1)))
#define pok_direction_orthog1(dir) (dir==pok_direction_up || dir==pok_direction_down ? (int)pok_direction_left \
        : (dir==pok_direction_left || dir==pok_direction_right ? pok_direction_up : pok_direction_none))
#define pok_direction_orthog2(dir) (dir==pok_direction_up || dir==pok_direction_down ? (int)pok_direction_right \
        : (dir==pok_direction_left || dir==pok_direction_right ? pok_direction_down : pok_direction_none))

#endif
