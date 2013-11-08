#ifndef LICE_HDR
#define LICE_HDR
#include "util.h"
#include "ast.h"
#include "amd64.h"

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_data_section(void);
void gen_function(ast_t *function);

// parse
list_t *parse_run(void);

#endif
