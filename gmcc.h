#ifndef GMCC_HDR
#define GMCC_HDR
#include <stdio.h>
#include <stdbool.h>

// config
#define GMCC_ENTRY     "gmcc_entry"
#define GMCC_ASSEMBLER "as -o blob.o"
#define GMCC_LINKER    "gcc blob.o invoke.c -o program"

// lexer.c

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
    };
} lexer_token_t;


typedef struct string_s string_t;
string_t *string_create(void);
char *string_buffer(string_t *string);
void string_append(string_t *string, char ch);
void string_appendf(string_t *string, const char *fmt, ...);
char *string_quote(char *p);

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
    AST_TYPE_LITERAL,
    AST_TYPE_VAR,

    // function stuff
    AST_TYPE_CALL,

    // misc
    AST_TYPE_DECL,

    // pointer stuff
    AST_TYPE_ADDR,
    AST_TYPE_DEREF

} ast_type_t;

// language types
typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_ARRAY,
    TYPE_PTR
} type_t;

typedef struct data_type_s data_type_t;
struct data_type_s {
    type_t       type;
    data_type_t *pointer;
};


// the ast and ast node and everything structures
// this is how ast should be done, one structure
// to rule them all!
struct ast_s {
    char         type;
    data_type_t *ctype; // C type

    // node crap occupies same memory location
    // to keep ram footprint minimal
    union {
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

        // unary
        struct {
            ast_t *operand;
        } unary;

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

data_type_t *ast_data_int(void);
data_type_t *ast_data_char(void);
data_type_t *ast_data_array(void);

ast_t *ast_new_unary(char type, data_type_t *data, ast_t *operand);
ast_t *ast_new_binary(char type, data_type_t *data, ast_t *left, ast_t *right);
ast_t *ast_new_string(char *value);
ast_t *ast_new_int(int value);
ast_t *ast_new_char(char value);
ast_t *ast_new_variable(data_type_t *type, char *name);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_func_call(char *name, int size, ast_t **nodes);

data_type_t *ast_new_pointer(data_type_t *type);
const char *ast_type_string(data_type_t *type);

// data singletons
ast_t *ast_strings(void);
ast_t *ast_variables(void);

// debug
char *ast_dump_string(ast_t *ast);

// parse.c
void parse_compile(FILE *as, int dump);

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_emit_data(FILE *as, ast_t *strings);
void gen_emit_expression(FILE *as, ast_t *ast);
#endif
