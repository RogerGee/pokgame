/* error.h - pokgame */
#ifndef POKGAME_ERROR_H
#define POKGAME_ERROR_H

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
    pok_ex_graphics
};

/* an exception is used by one module to report a runtime exception to another; each
   thread maintains its own stack of exceptions; an exception is popped off and 
   remains in memory until the next exception is popped off */
struct pok_exception
{
    int exID;
    enum pok_ex_kind category;
    const char* message;
};
void pok_exception_load();
void pok_exception_unload();
struct pok_exception* pok_exception_new();
const struct pok_exception* pok_exception_pop();

#endif
