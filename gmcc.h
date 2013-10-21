#ifndef GMCC_HDR
#define GMCC_HDR
#include <stdio.h>

// config
#define GMCC_ENTRY     "gmcc_entry"
#define GMCC_ASSEMBLER "as -o blob.o"
#define GMCC_LINKER    "gcc blob.o invoke.c -o program"

// var.c
typedef struct var_s var_t;

var_t *var_find(const char *name);
var_t *var_create(char *name);

struct var_s {
    char  *name;
    int    placement;
    var_t *next;
};

// ast.c
typedef struct ast_s ast_t;

// represents what type of ast node it is
typedef enum {
    // data storage
    ast_type_data_int,
    ast_type_data_var
} ast_type_t;

// the ast and ast node and everything structures
// this is how ast should be done, one structure
// to rule them all!
struct ast_s {
    char type;

    union {
        struct {
            int    integer;
            var_t *variable;
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
ast_t *ast_new_data_var(var_t *var);
void ast_dump(ast_t *ast);

// parse.c
void parse_compile(FILE *as, int dump);

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_emit_string(FILE *as, ast_t *ast);
void gen_emit_expression(FILE *as, ast_t *ast);
void gen_emit_bin(FILE *as, ast_t *ast);
#endif
