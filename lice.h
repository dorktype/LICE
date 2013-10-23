#ifndef LICE_HDR
#define LICE_HDR

#include <stdio.h>
#include <stdbool.h>
#include "util.h"

////////////////////////////////////////////////////////////////////////
// lexer
typedef enum {
    LEXER_TOKEN_IDENT,
    LEXER_TOKEN_PUNCT,
    LEXER_TOKEN_INT,
    LEXER_TOKEN_CHAR,
    LEXER_TOKEN_STRING,

    // reclassified tokens need to not conflict with
    // the ast types
    LEXER_TOKEN_EQUAL     = 0x200,
    LEXER_TOKEN_INCREMENT,
    LEXER_TOKEN_DECREMENT
} lexer_token_type_t;

typedef struct {
    lexer_token_type_t type;
    union {
        int   integer;
        int   punct;
        char *string;
        char  character;
    };
} lexer_token_t;


bool lexer_ispunct(lexer_token_t *token, int c);
void lexer_unget(lexer_token_t *token);
lexer_token_t *lexer_next(void);
lexer_token_t *lexer_peek(void);
char *lexer_tokenstr(lexer_token_t *token);

////////////////////////////////////////////////////////////////////////
// ast

typedef struct ast_s ast_t;

typedef enum {
    // data storage
    AST_TYPE_LITERAL   = 0x100,
    AST_TYPE_STRING,
    AST_TYPE_VAR_LOCAL,
    AST_TYPE_VAR_GLOBAL,

    // function stuff
    AST_TYPE_CALL,
    AST_TYPE_FUNCTION,

    // misc
    AST_TYPE_DECL,
    AST_TYPE_ARRAY_INIT,

    // pointer stuff
    AST_TYPE_ADDRESS,
    AST_TYPE_DEREFERENCE,

    // statements
    AST_TYPE_STATEMENT_IF,
    AST_TYPE_STATEMENT_FOR,
    AST_TYPE_STATEMENT_RETURN
} ast_type_t;


// language types
typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_ARRAY,
    TYPE_POINTER
} type_t;

typedef struct data_type_s data_type_t;

struct data_type_s {
    type_t       type;
    data_type_t *pointer;
    int          size;
};

typedef struct {
    char *data;
    char *label;
} ast_string_t;

typedef struct  {
    char *name;
    int   off;
} ast_local_t;

typedef struct {
    char *name;
    char *label;
} ast_global_t;

typedef struct {
    char *name;

    struct {
        list_t *args;
    } call;

    list_t *params;
    list_t *locals;
    list_t *body;
} ast_function_t;

typedef struct {
    ast_t *operand;
} ast_unary_t;


typedef struct {
    ast_t *var;
    ast_t *init;
} ast_decl_t;

typedef struct {
    ast_t  *cond;
    list_t *then;
    list_t *last;
} ast_ifthan_t;

typedef struct {
    ast_t  *init;
    ast_t  *cond;
    ast_t  *step;
    list_t *body;
} ast_for_t;

struct ast_s {
    int           type;
    data_type_t *ctype;

    // all the possible nodes
    union {
        int            integer;         // integer
        char           character;       // character
        ast_string_t   string;          // string
        ast_local_t    local;           // local variables
        ast_global_t   global;          // global variables
        ast_function_t function;        // function
        ast_unary_t    unary;           // unary operations
        ast_decl_t     decl;            // declarations
        ast_ifthan_t   ifstmt;          // if statement
        ast_for_t      forstmt;         // for statement
        ast_t         *returnstmt;      // return statement
        list_t        *array;           // array initializer
        struct {                        // tree
            ast_t *left;
            ast_t *right;
        };
    };
};

ast_t *ast_new_unary(int type, data_type_t *data, ast_t *operand);
ast_t *ast_new_binary(int type, ast_t *left, ast_t *right);
ast_t *ast_new_int(int value);
ast_t *ast_new_char(char value);
char *ast_new_label(void);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_variable_local(data_type_t *type, char *name);
ast_t *ast_new_reference_local(data_type_t *type, ast_t *var, int off);
ast_t *ast_new_variable_global(data_type_t *type, char *name, bool file);
ast_t *ast_new_reference_global(data_type_t *type, ast_t *var, int off);
ast_t *ast_new_string(char *value);
ast_t *ast_new_call(data_type_t *type, char *name, list_t *args);
ast_t *ast_new_function(data_type_t *type, char *name, list_t *params, list_t *body, list_t *locals);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_array_init(list_t *init);
ast_t *ast_new_if(ast_t *cond, list_t *then, list_t *last);
ast_t *ast_new_for(ast_t *init, ast_t *cond, ast_t *step, list_t *body);
ast_t *ast_new_return(ast_t *val);

ast_t *ast_find_variable(const char *name);

data_type_t *ast_new_pointer(data_type_t *type);
data_type_t *ast_new_array(data_type_t *type, int size);
data_type_t *ast_array_convert(data_type_t *ast);

data_type_t *ast_result_type(char op, data_type_t *a, data_type_t *b);


// data
extern data_type_t *ast_data_int;
extern data_type_t *ast_data_char;

extern list_t      *ast_globals;
extern list_t      *ast_locals;
extern list_t      *ast_params;

// debug
char *ast_string(ast_t *ast);
char *ast_block_string(list_t *block);

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_data_section(void);
void gen_function(ast_t *function);

// parse
list_t *parse_function_list(void);

#endif
