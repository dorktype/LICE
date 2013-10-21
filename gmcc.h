#ifndef GMCC_HDR
#define GMCC_HDR
#include <stdio.h>

// config
#define GMCC_ENTRY_INT "gmcc_entry_int"
#define GMCC_ENTRY_STR "gmcc_entry_str"
#define GMCC_ASSEMBLER "as -o blob.o"
#define GMCC_LINKER    "gcc blob.o invoke.c -o program"

// ast.c
typedef struct ast_s ast_t;

// represents what type of ast node it is
typedef enum {
    // data storage
    ast_type_data_int,
    ast_type_data_str
} ast_type_t;

// the ast and ast node and everything structures
// this is how ast should be done, one structure
// to rule them all!
struct ast_s {
    char type;

    union {
        struct {
            int   integer;
            char *string;
        } value;

        struct {
            ast_t *left;
            ast_t *right;
        };
    };
};

ast_t *ast_new_bin_op(char type, ast_t *left, ast_t *right);
ast_t *ast_new_data_str(char *value);
ast_t *ast_new_data_int(int value);
void ast_dump(ast_t *ast);

// parse.c
ast_t *parse(void);

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_emit_string(FILE *as, ast_t *ast);
void gen_emit_expression(FILE *as, ast_t *ast);
void gen_emit_bin(FILE *as, ast_t *ast);
#endif
