#ifndef LICE_AST_HDR
#define LICE_AST_HDR

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
    AST_TYPE_PROTOTYPE,

    // misc
    AST_TYPE_DECLARATION,
    AST_TYPE_INITIALIZERLIST,
    AST_TYPE_STRUCT,

    // pointer stuff
    AST_TYPE_ADDRESS,
    AST_TYPE_DEREFERENCE,

    // expression
    AST_TYPE_EXPRESSION_TERNARY,
    AST_TYPE_EXPRESSION_CAST,

    // statements
    AST_TYPE_STATEMENT_IF,
    AST_TYPE_STATEMENT_FOR,
    AST_TYPE_STATEMENT_WHILE,
    AST_TYPE_STATEMENT_DO,
    AST_TYPE_STATEMENT_SWITCH,
    AST_TYPE_STATEMENT_CASE,
    AST_TYPE_STATEMENT_DEFAULT,
    AST_TYPE_STATEMENT_RETURN,
    AST_TYPE_STATEMENT_BREAK,
    AST_TYPE_STATEMENT_CONTINUE,
    AST_TYPE_STATEMENT_COMPOUND,
    AST_TYPE_STATEMENT_GOTO,
    AST_TYPE_STATEMENT_LABEL,

    // pre/post increment ast
    AST_TYPE_POST_INCREMENT,
    AST_TYPE_POST_DECREMENT,
    AST_TYPE_PRE_INCREMENT,
    AST_TYPE_PRE_DECREMENT
} ast_type_t;


// language types (parser) to move
typedef enum {
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LLONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_LDOUBLE,
    TYPE_ARRAY,
    TYPE_POINTER,
    TYPE_STRUCTURE,
    TYPE_FUNCTION,

    // other language types are only used as part of the
    // parser or ast
    TYPE_CDECL
} type_t;

// ast table data types
typedef enum {
    AST_DATA_VOID,
    AST_DATA_LONG,
    AST_DATA_LLONG,
    AST_DATA_INT,
    AST_DATA_SHORT,
    AST_DATA_CHAR,
    AST_DATA_FLOAT,
    AST_DATA_DOUBLE,
    AST_DATA_LDOUBLE,
    AST_DATA_ULONG,
    AST_DATA_ULLONG,
    AST_DATA_FUNCTION,

    AST_DATA_COUNT
} ast_data_type_t;


typedef enum {
    CDECL_BODY = 1,
    CDECL_PARAMETER,
    CDECL_TYPEONLY,
    CDECL_CAST
} cdecl_t;

// storage class
typedef enum {
    STORAGE_TYPEDEF = 1,
    STORAGE_EXTERN,
    STORAGE_STATIC,
    STORAGE_AUTO,
    STORAGE_REGISTER
} storage_t;

typedef struct data_type_s data_type_t;

struct data_type_s {
    type_t       type;
    int          size;
    bool         sign;    // signed?
    int          length;
    data_type_t *pointer;

    // structure
    struct {
        table_t *fields;
        int      offset;
    };

    // function
    struct {
        data_type_t *returntype;
        list_t      *parameters;
        bool         hasdots;
    };
};

typedef struct {
    char *data;
    char *label;
} ast_string_t;

typedef struct {
    char *name;
    int   off;
    char *label;
} ast_variable_t;

typedef struct {
    char *name;

    struct {
        list_t *args;
        list_t *paramtypes;
    } call;

    // decl todo make struct named
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

typedef struct {
    list_t      *list;
    data_type_t *type;
} ast_initlist_t;

typedef struct {
    ast_t       *expr;
    ast_t       *body;
} ast_switch_t;

typedef struct {
    char *label;
    char *where;
} ast_goto_t;

struct ast_s {
    int           type;
    data_type_t *ctype;

    // all the possible nodes
    union {
        int             casevalue;      // switch case value
        long            integer;        // integer
        char            character;      // character
        ast_string_t    string;         // string
        ast_variable_t  variable;       // local and global variable
        ast_function_t  function;       // function
        ast_unary_t     unary;          // unary operations
        ast_decl_t      decl;           // declarations
        ast_ifthan_t    ifstmt;         // if statement
        ast_for_t       forstmt;        // for statement
        ast_switch_t    switchstmt;     // switch statement
        ast_t          *returnstmt;     // return statement
        list_t         *compound;       // compound statement
        ast_initlist_t  initlist;       // initializer list
        ast_goto_t      gotostmt;       // goto statement

        struct {                        // tree
            ast_t *left;
            ast_t *right;
        };

        struct {                        // struct
            ast_t       *structure;
            char        *field;
            data_type_t *fieldtype;
        };

        struct {                        // float or double
            double value;
            char  *label;
        } floating;

    };
};

ast_t *ast_structure_reference_new(data_type_t *type, ast_t *structure, char *name);
data_type_t *ast_structure_field_new(data_type_t *type, int offset);
data_type_t *ast_structure_new(table_t *fields, int size);

ast_t *ast_new_unary(int type, data_type_t *data, ast_t *operand);
ast_t *ast_new_binary(int type, ast_t *left, ast_t *right);
ast_t *ast_new_integer(data_type_t *type, int value);
ast_t *ast_new_floating(data_type_t *, double value);
ast_t *ast_new_char(char value);
char *ast_label(void);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_variable_local(data_type_t *type, char *name);
ast_t *ast_new_reference_local(data_type_t *type, ast_t *var, int off);
ast_t *ast_new_variable_global(data_type_t *type, char *name);
ast_t *ast_new_reference_global(data_type_t *type, ast_t *var, int off);
ast_t *ast_new_string(char *value);
ast_t *ast_new_call(data_type_t *type, char *name, list_t *args, list_t *paramtypes);
ast_t *ast_new_function(data_type_t *type, char *name, list_t *params, ast_t *body, list_t *locals);
ast_t *ast_new_decl(ast_t *var, ast_t *init);
ast_t *ast_new_initializerlist(list_t *init);
ast_t *ast_new_if(ast_t *cond, ast_t *then, ast_t *last);
ast_t *ast_new_for(ast_t *init, ast_t *cond, ast_t *step, ast_t *body);
ast_t *ast_new_while(ast_t *cond, ast_t *body);
ast_t *ast_new_do(ast_t *cond, ast_t *body);
ast_t *ast_new_return(data_type_t *returntype, ast_t *val);
ast_t *ast_new_compound(list_t *statements);
ast_t *ast_new_ternary(data_type_t *type, ast_t *cond, ast_t *then, ast_t *last);
ast_t *ast_new_switch(ast_t *expr, ast_t *body);
ast_t *ast_new_case(int value);
ast_t *ast_label_new(char *);
ast_t *ast_goto_new(char *);

ast_t *ast_make(int type);

data_type_t *ast_new_prototype(data_type_t *returntype, list_t *paramtypes, bool dots);
data_type_t *ast_new_pointer(data_type_t *type);
data_type_t *ast_new_array(data_type_t *type, int size);
data_type_t *ast_array_convert(data_type_t *ast);

data_type_t *ast_result_type(char op, data_type_t *a, data_type_t *b);

const char *ast_type_string(data_type_t *type);

bool ast_type_integer(data_type_t *type);
bool ast_type_floating(data_type_t *type);
data_type_t *ast_type_copy(data_type_t *type);
data_type_t *ast_type_create(type_t type, bool sign);
data_type_t *ast_type_stub(void);


extern data_type_t *ast_data_table[AST_DATA_COUNT];

// data
extern list_t      *ast_floats;
extern list_t      *ast_strings;
extern list_t      *ast_locals;
extern list_t      *ast_gotos;
extern table_t     *ast_globalenv;
extern table_t     *ast_localenv;
extern table_t     *ast_structures;
extern table_t     *ast_unions;
extern table_t     *ast_labels;

// debug
char *ast_string(ast_t *ast);

#endif
