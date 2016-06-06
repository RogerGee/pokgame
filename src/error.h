/* error.h - pokgame */
#ifndef POKGAME_ERROR_H
#define POKGAME_ERROR_H
#include "types.h"

/* this error functionality is used to report errors to the process's
   stderr channel; they are used primarily to test assertions */
enum pok_errorkind
{
    pok_error_message, /* the error is merely just a message the program wants to log to its stderr */
    pok_error_warning, /* the error doesn't effect the operation of the process but was not expected */
    pok_error_fatal, /* the error is un-recoverable and the process will terminate */
    pok_error_unimplemented /* the control tried to execute a feature not yet supported */
};
void pok_error(enum pok_errorkind kind,const char* message, ...);
void pok_error_fromstack(enum pok_errorkind kind);
#define pok_message(s, ...) pok_error(pok_error_message, s, __VA_ARGS__) /* note: variadic macro is C99 */
#ifdef POKGAME_DEBUG
#define pok_assert(expr) if (!(expr)) pok_error(pok_error_fatal,"assertion failed: %s",#expr)
#else
#define pok_assert(expr) (void) (0) /* do a noop */
#endif

/* exception categories are enumerated by the modules that generate them */
enum pok_ex_kind
{
    pok_ex_default,
    pok_ex_net,
    pok_ex_netobj,
    pok_ex_graphics,
    pok_ex_image,
    pok_ex_tileman,
    pok_ex_spriteman,
    pok_ex_tile,
    pok_ex_map,
    _pok_ex_top
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
   thread maintains its own last exception; when an exception is popped off, it remains
   in memory until a new exception is created; only one exception is allowed per thread */
struct pok_exception
{
    int id;
    enum pok_ex_kind kind;
    char message[256];
};
struct pok_exception* pok_exception_new();
struct pok_exception* pok_exception_new_ex(enum pok_ex_kind kind,int id);
struct pok_exception* pok_exception_new_format(const char* message, ...);
bool_t pok_exception_check();
bool_t pok_exception_check_ex(enum pok_ex_kind kind,int id);
const struct pok_exception* pok_exception_pop();
const struct pok_exception* pok_exception_pop_ex(enum pok_ex_kind kind,int id);
const struct pok_exception* pok_exception_peek();
const struct pok_exception* pok_exception_peek_ex(enum pok_ex_kind kind,int id);
void pok_exception_load_message(struct pok_exception* except);
void pok_exception_append_message(struct pok_exception* except,const char* message, ...);

struct pok_datetime
{
    int second;
    int minute;
    int hour;
    int wday;
    int mday;
    int month;
    int year;
};
void pok_datetime_init(struct pok_datetime* datetime);

#endif
