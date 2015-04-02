/* error.h - pokgame */
#ifndef POKGAME_ERROR_H
#define POKGAME_ERROR_H
#include "types.h"

/* this error functionality is used to report errors to the process's
   stderr channel; they are used primarily to test assertions */
enum pok_errorkind
{
    pok_error_warning, /* the error doesn't effect the operation of the process but was not expected */
    pok_error_fatal, /* the error is un-recoverable and the process will terminate */
    pok_error_unimplemented /* the control tried to execute a feature not yet supported */
};
void pok_error(enum pok_errorkind kind,const char* message);

/* exception categories are enumerated by the modules that generate them */
enum pok_ex_kind
{
    pok_ex_default,
    pok_ex_net,
    pok_ex_graphics,
    pok_ex_image,
    pok_ex_tile
};

enum pok_ex_default
{
    pok_ex_default_undocumented,
    pok_ex_default_memory_allocation_fail
};

/* module functions */
void pok_exception_load_module();
void pok_exception_flag_memory_error();
void pok_exception_unload_module();

/* an exception is used by one module to report a runtime exception to another; each
   thread maintains its own stack of exceptions; an exception is popped off and 
   remains in memory until the next exception is popped off */
struct pok_exception
{
    int id;
    enum pok_ex_kind kind;
    const char* message;
};
struct pok_exception* pok_exception_new();
struct pok_exception* pok_exception_new_ex(enum pok_ex_kind kind,int id);
bool_t pok_exception_check();
bool_t pok_exception_check_ex(enum pok_ex_kind kind);
const struct pok_exception* pok_exception_pop();
const struct pok_exception* pok_exception_pop_ex(enum pok_ex_kind kind);
const struct pok_exception* pok_exception_peek();
const struct pok_exception* pok_exception_peek_ex(enum pok_ex_kind kind);

#endif
