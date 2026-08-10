/*
 * RCC - RinOS C Compiler
 * Preprocessor Header
 */

#ifndef PREPROC_H
#define PREPROC_H

#include "rcc.h"

/* Maximum include depth */
#define PP_MAX_INCLUDE_DEPTH 32

/* Maximum macro parameters */
#define PP_MAX_PARAMS 32

/* Macro definition */
typedef struct Macro {
    const char* name;
    const char* value;           /* For object-like macros */
    const char** params;         /* Parameter names for function-like */
    int param_count;             /* -1 for object-like macros */
    const char* body;            /* Body for function-like macros */
    bool is_builtin;
    struct Macro* next;
} Macro;

/* Preprocessor state */
typedef struct {
    Macro* macros;               /* Macro hash table head */
    const char** include_paths;  /* Include search paths */
    int include_path_count;
    int include_depth;           /* Current include nesting depth */

    /* Conditional compilation stack */
    struct {
        bool active;             /* Currently in active branch */
        bool had_true;           /* Already had a true branch */
        bool in_else;            /* Already seen #else */
    } cond_stack[64];
    int cond_depth;
    Macro* expanding[64];
    int expansion_depth;
    const char** dependencies;
    int dependency_count;
    int dependency_capacity;
} Preprocessor;

/* Initialize preprocessor */
Preprocessor* pp_new(void);
void pp_free(Preprocessor* pp);

/* Add include path */
void pp_add_include_path(Preprocessor* pp, const char* path);

/* Define/undefine macros */
void pp_define(Preprocessor* pp, const char* name, const char* value);
void pp_define_func(Preprocessor* pp, const char* name, const char** params, int param_count, const char* body);
void pp_undef(Preprocessor* pp, const char* name);
bool pp_is_defined(Preprocessor* pp, const char* name);
Macro* pp_get_macro(Preprocessor* pp, const char* name);

/* Process source file - returns preprocessed source */
char* pp_process_file(Preprocessor* pp, const char* filename);
char* pp_process_string(Preprocessor* pp, const char* source, const char* filename);
bool pp_write_dependencies(Preprocessor* pp, const char* target,
                           const char* source, const char* dependency_file);

#endif /* PREPROC_H */
