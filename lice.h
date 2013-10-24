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
    LEXER_TOKEN_LEQUAL,
    LEXER_TOKEN_GEQUAL,
    LEXER_TOKEN_INCREMENT,
    LEXER_TOKEN_DECREMENT,
    LEXER_TOKEN_ARROW,

    // logical, not bitwise
    LEXER_TOKEN_AND,
    LEXER_TOKEN_OR
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
typedef struct env_s env_t;

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
    AST_TYPE_DECLARATION,
    AST_TYPE_ARRAY_INIT,
    AST_TYPE_STRUCT,

    // pointer stuff
    AST_TYPE_ADDRESS,
    AST_TYPE_DEREFERENCE,

    // expression
    AST_TYPE_EXPRESSION_TERNARY,

    // statements
    AST_TYPE_STATEMENT_IF,
    AST_TYPE_STATEMENT_FOR,
    AST_TYPE_STATEMENT_RETURN,
    AST_TYPE_STATEMENT_COMPOUND
} ast_type_t;


// language types
typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_ARRAY,
    TYPE_POINTER,
    TYPE_STRUCTURE
} type_t;

typedef struct data_type_s data_type_t;

struct data_type_s {
    type_t       type;
    data_type_t *pointer;
    int          size;

    // structure
    struct {
        char        *name;
        char        *tag;
        list_t      *fields;
        int          offset;
    };
};

typedef struct {
    char *data;
    char *label;
} ast_string_t;

typedef struct {
    char *name;
    struct {
        int   off;
        char *label;
    };
} ast_variable_t;

typedef struct {
    char *name;

    struct {
        list_t *args;
    } call;

    list_t *params;
    list_t *locals;
    ast_t  *body;
} ast_function_t;

typedef struct {
    ast_t *operand;
} ast_unary_t;


typedef struct {
    ast_t *var;
    ast_t *init;
} ast_decl_t;

// While this named ifthan it's also used for ternary expressions
// mostly because they evaluate to the same thing in the AST.
typedef struct {
    ast_t  *cond;
    ast_t  *then;
    ast_t  *last;
} ast_ifthan_t;

typedef struct {
    ast_t  *init;
    ast_t  *cond;
    ast_t  *step;
    ast_t  *body;
} ast_for_t;

// a scopes enviroment is represented as a list of variables
// and a pointer to the next enviroment within it.
struct env_s {
    list_t *variables;
    list_t *structures;
    env_t  *next;
};

struct ast_s {
    int           type;
    data_type_t *ctype;

    // all the possible nodes
    union {
        int            integer;         // integer
        char           character;       // character
        ast_string_t   string;          // string
        ast_variable_t variable;        // local and global variable
        ast_function_t function;        // function
        ast_unary_t    unary;           // unary operations
        ast_decl_t     decl;            // declarations
        ast_ifthan_t   ifstmt;          // if statement
        ast_for_t      forstmt;         // for statement
        ast_t         *returnstmt;      // return statement
        list_t        *compound;        // compound statement
        list_t        *array;           // array initializer
        struct {                        // tree
            ast_t *left;
            ast_t *right;
        };
        struct {                        // struct
            ast_t       *structure;
            data_type_t *field;
        };
    };
};

ast_t *ast_structure_reference_new(ast_t *structure, data_type_t *field);
data_type_t *ast_structure_field_new(data_type_t *type, char *name, int offset);
data_type_t *ast_structure_new(list_t *fields, char *tag);

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
ast_t *ast_new_function(data_type_t *type, char *name, list_t *params, ast_t *body, list_t *locals);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_array_init(list_t *init);
ast_t *ast_new_if(ast_t *cond, ast_t *then, ast_t *last);
ast_t *ast_new_for(ast_t *init, ast_t *cond, ast_t *step, ast_t *body);
ast_t *ast_new_return(ast_t *val);
ast_t *ast_new_compound(list_t *statements);
ast_t *ast_new_ternary(data_type_t *type, ast_t *cond, ast_t *then, ast_t *last);

// search
ast_t *ast_find_variable(const char *name);
data_type_t *ast_find_structure_field(data_type_t *structure, const char *name);
data_type_t *ast_find_structure_definition(const char *name);

data_type_t *ast_new_pointer(data_type_t *type);
data_type_t *ast_new_array(data_type_t *type, int size);
data_type_t *ast_array_convert(data_type_t *ast);

data_type_t *ast_result_type(char op, data_type_t *a, data_type_t *b);

int ast_sizeof(data_type_t *type);

// data
extern data_type_t *ast_data_int;
extern data_type_t *ast_data_char;
extern env_t       *ast_globalenv;
extern env_t       *ast_localenv;
extern list_t      *ast_localvars;
extern list_t      *ast_structures;

// enviroment handling
env_t *ast_env_new(env_t *next);
void  ast_env_push(env_t *env, ast_t *var);

// debug
char *ast_string(ast_t *ast);

// gmcc.c
void compile_error(const char *fmt, ...);

// gen.c
void gen_data_section(void);
void gen_function(ast_t *function);

// parse
list_t *parse_function_list(void);

#endif
