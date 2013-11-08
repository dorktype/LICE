#ifndef LICE_HDR
#define LICE_HDR
#include "util.h"
#include "ast.h"
#include "amd64.h"

void compile_error(const char *fmt, ...);

void gen_data_section(void);
void gen_function(ast_t *function);

list_t *parse_run(void);

#endif
