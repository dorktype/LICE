#ifndef GMCC_HDR
#define GMCC_HDR
#include <stdio.h>
#include <stdbool.h>

// config
#define GMCC_ENTRY     "gmcc_entry"
#define GMCC_ASSEMBLER "as -o blob.o"
#define GMCC_LINKER    "gcc blob.o invoke.c -o program"

// lexer.c
typedef struct string_s string_t;
typedef enum {
    LEXER_TOKEN_IDENT,
    LEXER_TOKEN_PUNCT,
    LEXER_TOKEN_INT,
    LEXER_TOKEN_CHAR,
    LEXER_TOKEN_STRING
} lexer_token_type_t;

typedef struct {
    lexer_token_type_t type;
    union {
        int   integer;
        char *string;
        char  punct;
        char  character;
    } value;
} lexer_token_t;

bool lexer_ispunc(lexer_token_t *token, char c);
void lexer_unget(lexer_token_t *token);
lexer_token_t *lexer_next(void);
lexer_token_t *lexer_peek(void);
char *lexer_tokenstr(lexer_token_t *token);

// ast.c
typedef struct ast_s ast_t;

// var.c
ast_t *var_find(const char *name);

// ast.c
// represents what type of ast node it is
typedef enum {
    // data storage
    ast_type_data_int,
    ast_type_data_var,
    ast_type_data_str,
    ast_type_data_chr,

    // misc
    ast_type_decl,

    // function stuff
    ast_type_func_call
} ast_type_t;

// language types
typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_STR
} ctype_t;

// the ast and ast node and everything structures
// this is how ast should be done, one structure
// to rule them all!
struct ast_s {
    char    type;
    ctype_t ctype; // C type

    // node crap occupies same memory location
    // to keep ram footprint minimal
    union {
        // data
        struct {
            int    integer;
            char   character;

            // variable
            struct {
                char  *name;
                int    placement;
                ast_t *next;
            } variable;

            // string
            struct {
                char  *data;
                int    id;
                ast_t *next;
            } string;

            // function call
            struct {
                char  *name;
                int   size; // # of arguments
                ast_t **args;
            } call;
        } value;

        // tree
        struct {
            ast_t *left;
            ast_t *right;
        };

        // declaration tree
        struct {
            ast_t *var;
            ast_t *init;
        } decl;
    };
};

ast_t *ast_new_bin_op(char type, ast_t *left, ast_t *right);
ast_t *ast_new_data_str(char *value);
ast_t *ast_new_data_int(int value);
ast_t *ast_new_data_chr(char value);
ast_t *ast_new_data_var(ctype_t type, char *name);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_func_call(char *name, int size, ast_t **nodes);

// data singletons
ast_t *ast_strings(void);
ast_t *ast_variables(void);
void ast_dump(ast_t *ast);

// parse.c
void parse_compile(FILE *as, int dump);

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_emit_data(FILE *as, ast_t *strings);
void gen_emit_expression(FILE *as, ast_t *ast);
void gen_emit_bin(FILE *as, ast_t *ast);
#endif
