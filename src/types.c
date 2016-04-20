#include "types.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

/* external constants */
const struct pok_point ORIGIN = {0,0};

/* pok_string */
static char dummyBuffer[16];
struct pok_string* pok_string_new()
{
    struct pok_string* str = malloc(sizeof(struct pok_string));
    if (str == NULL)
        return NULL;
    pok_string_init(str);
    return str;
}
struct pok_string* pok_string_new_ex(size_t initialCapacity)
{
    struct pok_string* str = malloc(sizeof(struct pok_string));
    if (str == NULL)
        return NULL;
    pok_string_init_ex(str,initialCapacity);
    return str;
}
void pok_string_free(struct pok_string* str)
{
    pok_string_delete(str);
    free(str);
}
void pok_string_init(struct pok_string* str)
{
    str->len = 0;
    str->cap = sizeof(dummyBuffer);
    str->buf = malloc(str->cap);
    if (str->buf == NULL) {
        pok_error(pok_error_warning,"failed memory allocation: pok_string");
        str->buf = dummyBuffer;
    }
    str->buf[0] = '\0';
}
void pok_string_init_ex(struct pok_string* str,size_t initialCapacity)
{
    str->len = 0;
    str->cap = initialCapacity;
    str->buf = malloc(str->cap);
    if (str->buf == NULL) {
        pok_error(pok_error_warning,"failed memory allocation: pok_string");
        str->buf = dummyBuffer;
    }
    str->buf[0] = '\0';
}
void pok_string_delete(struct pok_string* str)
{
    if (str->buf != dummyBuffer)
        free(str->buf);
    str->len = 0;
    str->cap = 0;
}
static bool_t pok_string_realloc(struct pok_string* str,size_t hint)
{
    char* newbuf;
    size_t newcap;
    if (str->buf == dummyBuffer)
        return FALSE;
    /* find a new capacity for the buffer; it should be a multiple of two */
    newcap = str->cap;
    do {
        newcap <<= 1;
    } while (newcap < hint);
    newbuf = realloc(str->buf,newcap);
    if (newbuf == NULL) {
        /* treat a failed allocation as a non-fatal error; the string will silently
           fail its operations and the program will have undefined behavior */
        pok_error(pok_error_warning,"failed memory allocation: pok_string");
        return FALSE;
    }
    str->buf = newbuf;
    str->cap = newcap;
    return TRUE;
}
void pok_string_assign(struct pok_string* str,const char* s)
{
    size_t len = strlen(s);
    size_t zlen = len+1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf,s,zlen);
        str->len = len;
    }
}
void pok_string_assign_ex(struct pok_string* str,const char* s,size_t length)
{
    /* don't assume the user provided a null-terminator */
    size_t zlen = length+1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf,s,length); /* copy just the specified number of bytes */
        str->buf[length] = 0; /* provide the null terminator ourselves */
        str->len = length;
    }
}
void pok_string_copy(struct pok_string* str,const struct pok_string* operand)
{
    size_t zlen = operand->len + 1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf,operand->buf,zlen);
        str->len = operand->len;
    }
}
void pok_string_concat(struct pok_string* str,const char* s)
{
    size_t l = strlen(s);
    size_t len = str->len + l;
    size_t zlen = len + 1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf+str->len,s,l+1); /* add 1 to copy 0 byte */
        str->len = len;
    }
}
void pok_string_concat_ex(struct pok_string* str,const char* s,size_t length)
{
    /* don't assume 's' is null-terminated */
    size_t len = length + str->len;
    size_t zlen = len + 1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf+str->len,s,length);
        str->len = len;
        str->buf[len] = 0;
    }
}
void pok_string_concat_obj(struct pok_string* str,const struct pok_string* operand)
{
    size_t len;
    size_t zlen;
    len = str->len + operand->len;
    zlen = len+1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf+str->len,operand->buf,operand->len+1);
        str->len = len;
    }
}
void pok_string_concat_char(struct pok_string* str,char c)
{
    size_t len = str->len+2;
    if (len<=str->cap || pok_string_realloc(str,len)) {
        str->buf[str->len++] = c;
        str->buf[str->len] = '\0';
    }
}
void pok_string_clear(struct pok_string* str)
{
    str->len = 0;
    str->buf[0] = '\0';
}
void pok_string_reset(struct pok_string* str)
{
    str->len = 0;
    pok_string_realloc(str,16);
}

/* pok_location */
int pok_location_compar(const struct pok_location* left,const struct pok_location* right)
{
    /* similar to pok_point_compar() */
    if (left->column < right->column)
        return -1;
    if (left->column > right->column)
        return 1;
    if (left->row < right->row)
        return -1;
    if (left->row > right->row)
        return 1;
    return 0;
}
bool_t pok_location_test(const struct pok_location* pos,uint16_t column,uint16_t row)
{
    return pos->column == column && pos->row == row;
}

/* pok_point */
struct pok_point* pok_point_new(int32_t X,int32_t Y)
{
    struct pok_point* pnt = malloc(sizeof(struct pok_point));
    if (pnt != NULL) {
        pnt->X = X;
        pnt->Y = Y;
    }
    return pnt;
}
struct pok_point* pok_point_new_copy(const struct pok_point* point)
{
    struct pok_point* pnt = malloc(sizeof(struct pok_point));
    if (pnt != NULL) {
        pnt->X = point->X;
        pnt->Y = point->Y;
    }
    return pnt;
}
int pok_point_compar(const struct pok_point* left,const struct pok_point* right)
{
    /* compare points by X-coordinates; break ties with Y-coordinates */
    if (left->X < right->X)
        return -1;
    if (left->X > right->X)
        return 1;
    if (left->Y < right->Y)
        return -1;
    if (left->Y > right->Y)
        return 1;
    return 0;
}
bool_t pok_point_test(const struct pok_point* point,int x,int y)
{
    return point->X == x && point->Y == y;
}

/* pok_direction_operations */
int pok_direction_cycle_distance(enum pok_direction start,enum pok_direction end,int times,bool_t clockwise)
{
    /* compute how many moves it takes to get from 'start' direction
       to 'end' to do at least 'times' number of cycles */
    int c = 0;
    if (start!=pok_direction_none && end!=pok_direction_none) {
        do {
            if (clockwise)
                start = pok_direction_clockwise_next(start);
            else
                start = pok_direction_counterclockwise_next(start);
            ++c;
        } while (start != end);
        if (times > 0)
            c += (times - c/4) * 4;
    }
    return c;
}
void pok_direction_add_to_point(enum pok_direction dir,struct pok_point* point)
{
    switch (dir) {
    case pok_direction_up:
        point->Y -= 1;
        break;
    case pok_direction_down:
        point->Y += 1;
        break;
    case pok_direction_left:
        point->X -= 1;
        break;
    case pok_direction_right:
        point->X += 1;
        break;
    default:
        break;
    }
}
void pok_direction_add_to_location(enum pok_direction dir,struct pok_location* loc)
{
    /* the caller should be careful with this function (since 'column' and 'row'
       are unsigned integers) */
    switch (dir) {
    case pok_direction_up:
        loc->row -= 1;
        break;
    case pok_direction_down:
        loc->row += 1;
        break;
    case pok_direction_left:
        loc->column -= 1;
        break;
    case pok_direction_right:
        loc->column += 1;
        break;
    default:
        break;
    }
}
