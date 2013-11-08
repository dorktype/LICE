#ifndef LICE_HDR
#define LICE_HDR
#include "util.h"
#include "ast.h"

#ifdef LICE_TARGET_AMD64
#   include "arch_amd64.h"
#else
    /*
     * Any additional future targets will just keep bracing with
     * conditional inclusion here.
     */
#   include "arch_dummy.h"
#endif

/*
 * Function: compile_error
 *  Write compiler error diagnostic to stderr, formatted
 *
 * Parameters:
 *  fmt - Standard format specification string
 *  ... - Additional variable arguments
 *
 * Remarks:
 *  This function does not return, it kills execution via
 *  exit(1);
 */
void compile_error(const char *fmt, ...);


/* TODO: eliminate */
list_t *parse_run(void);
void gen_data_section(void);
void gen_function(ast_t *function);
#endif
