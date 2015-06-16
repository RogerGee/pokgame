#include "types.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

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
    size_t len = strlen(s) + str->len;
    size_t zlen = len+1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf+str->len,s,zlen);
        str->len = len;
    }
}
void pok_string_concat_ex(struct pok_string* str,const char* s,size_t length)
{
    size_t zlen;
    length += str->len;
    zlen = length+1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf+str->len,s,zlen);
        str->len = length;
    }
}
void pok_string_concat_obj(struct pok_string* str,const struct pok_string* operand)
{
    size_t len;
    size_t zlen;
    len = str->len + operand->len;
    zlen = len+1;
    if (zlen<=str->cap || pok_string_realloc(str,zlen)) {
        strncpy(str->buf+str->len,operand->buf,zlen);
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

/* pok_point */
int pok_point_compar(struct pok_point* left,struct pok_point* right)
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
