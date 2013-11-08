#ifndef LICE_AST_HDR
#define LICE_AST_HDR
/*
 * File: ast.h
 *  Implements the interface to LICE's abstract syntax tree
 */
typedef struct ast_s ast_t;

/*
 * Type: ast_type_t
 *  The type of ast node
 *
 *  Constants:
 *
 *  AST_TYPE_LITERAL                 - Literal
 *  AST_TYPE_STRING                  - String literal
 *  AST_TYPE_VAR_LOCAL               - Local variable
 *  AST_TYPE_VAR_GLOBAL              - Global variable
 *  AST_TYPE_CALL                    - Direct function call
 *  AST_TYPE_FUNCTION                - Function
 *  AST_TYPE_PROTOTYPE               - Prototype
 *  AST_TYPE_DECLARATION             - Declaration
 *  AST_TYPE_INITIALIZERLIST         - Initializer list
 *  AST_TYPE_STRUCT                  - Structure
 *  AST_TYPE_ADDRESS                 - Address of operation
 *  AST_TYPE_DEREFERENCE             - Pointer dereference
 *  AST_TYPE_EXPRESSION_TERNARY      - Ternary expression
 *  AST_TYPE_EXPRESSION_CAST         - Type cast expression
 *  AST_TYPE_STATEMENT_IF            - If statement
 *  AST_TYPE_STATEMENT_FOR           - For statement
 *  AST_TYPE_STATEMENT_WHILE         - While statement
 *  AST_TYPE_STATEMENT_DO            - Do statement
 *  AST_TYPE_STATEMENT_SWITCH        - Switch statement
 *  AST_TYPE_STATEMENT_CASE          - Switch statement case
 *  AST_TYPE_STATEMENT_DEFAULT       - Switch statement default case
 *  AST_TYPE_STATEMENT_RETURN        - Return statement
 *  AST_TYPE_STATEMENT_BREAK         - Break statement
 *  AST_TYPE_STATEMENT_CONTINUE      - Continue statement
 *  AST_TYPE_STATEMENT_COMPOUND      - Compound statement
 *  AST_TYPE_STATEMENT_GOTO          - Goto statement
 *  AST_TYPE_STATEMENT_LABEL         - Goto statement label
 *  AST_TYPE_POST_INCREMENT          - Post increment operation
 *  AST_TYPE_POST_DECREMENT          - Post decrement operation
 *  AST_TYPE_PRE_INCREMENT           - Pre increment operation
 *  AST_TYPE_PRE_DECREMENT           - Pre decrement operation
 *  AST_TYPE_LSHIFT                  - Left shift operation
 *  AST_TYPE_RSHIFT                  - Right shift operation
 *  AST_TYPE_EQUAL                   - Equality condition
 *  AST_TYPE_GEQUAL                  - Greater-or-equal condition
 *  AST_TYPE_LEQUAL                  - Less-or-equal condition
 *  AST_TYPE_NEQUAL                  - Not-equal condition
 *  AST_TYPE_AND                     - Logical-and operation
 *  AST_TYPE_OR                      - Logical-or operation
 */
typedef enum {
    AST_TYPE_LITERAL = 0x100,
    AST_TYPE_STRING,
    AST_TYPE_VAR_LOCAL,
    AST_TYPE_VAR_GLOBAL,
    AST_TYPE_CALL,
    AST_TYPE_FUNCTION,
    AST_TYPE_PROTOTYPE,
    AST_TYPE_DECLARATION,
    AST_TYPE_INITIALIZERLIST,
    AST_TYPE_STRUCT,
    AST_TYPE_ADDRESS,
    AST_TYPE_DEREFERENCE,
    AST_TYPE_EXPRESSION_TERNARY,
    AST_TYPE_EXPRESSION_CAST,
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
    AST_TYPE_POST_INCREMENT,
    AST_TYPE_POST_DECREMENT,
    AST_TYPE_PRE_INCREMENT,
    AST_TYPE_PRE_DECREMENT,
    AST_TYPE_LSHIFT,
    AST_TYPE_RSHIFT,
    AST_TYPE_EQUAL,
    AST_TYPE_GEQUAL,
    AST_TYPE_LEQUAL,
    AST_TYPE_NEQUAL,
    AST_TYPE_AND,
    AST_TYPE_OR
} ast_type_t;

/*
 * Type: type_t
 *  Type describing the ast type.
 *
 *  Constants:
 *
 *  TYPE_VOID       - void
 *  TYPE_CHAR       - char
 *  TYPE_SHORT      - short
 *  TYPE_INT        - int
 *  TYPE_LONG       - long
 *  TYPE_LLONG      - long long
 *  TYPE_DOUBLE     - double
 *  TYPE_LDOUBLE    - long double
 *  TYPE_ARRAY      - array (also contains a type_t for base type)
 *  TYPE_POINTER    - pointer (also contains a type_t for base type)
 *  TYPE_STRUCTURE  - structure (user defined)
 *  TYPE_FUNCTION   - function  (user defined)
 *  TYPE_CECL       - used by the parser for dealing with declarations
 */
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
    TYPE_CDECL
} type_t;

/*
 * Type: ast_data_type_t
 *  Type describing the indice into `ast_data_table`
 *
 *  Constants:
 *
 *  AST_DATA_VOID       - void
 *  AST_DATA_LONG       - long
 *  AST_DATA_LLONG      - long long
 *  AST_DATA_INT        - int
 *  AST_DATA_SHORT      - short
 *  AST_DATA_CHAR       - char
 *  AST_DATA_FLOAT      - float
 *  AST_DATA_DOUBLE     - double
 *  AST_DATA_LDOUBLE    - long double
 *  AST_DATA_ULONG      - unsigned long
 *  AST_DATA_ULLONG     - unsigned long long
 *  AST_DATA_FUNCTION   - function (current)
 */
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

/*
 * Type: cdecl_t
 *  Describes type of declarations
 *
 *  Constants:
 *
 *  CDECL_BODY          - function body
 *  CDECL_PARAMETER     - parameters (with name)
 *  CDECL_TYPEONLY      - parameters (without name)
 *  CDECL_CAST          - cast
 */
typedef enum {
    CDECL_BODY = 1,
    CDECL_PARAMETER,
    CDECL_TYPEONLY,
    CDECL_CAST
} cdecl_t;

/*
 * Type: storage_t
 *  Describes the storage class for a given variable
 *
 *  Constants:
 *
 *  STORAGE_TYPEDEF     - typedef to another type
 *  STORAGE_EXTERN      - external linkage
 *  STORAGE_STATIC      - static storage
 *  STORAGE_AUTO        - automatic storage (implicit)
 *  STORAGE_REGISTER    - make use of register for storage
 */
typedef enum {
    STORAGE_TYPEDEF = 1,
    STORAGE_EXTERN,
    STORAGE_STATIC,
    STORAGE_AUTO,
    STORAGE_REGISTER
} storage_t;

/*
 * Class: data_type_t
 *  A structure that describes a data type.
 */
typedef struct data_type_s data_type_t;
struct data_type_s {
    /*
     * Variable: type
     *  The type of the data type.
     *
     *  See <type_t> Constants for a list of
     *  valid constant values.
     */
    type_t type;

    /*
     * Variable: type
     *  The size of the given data data
     */
    int size;

    /*
     * Variable: sign
     *  Describes if the type is signed or unsigned.
     *
     *  Contains `true` when signed, otherwise `false.
     */
    bool sign;

    /*
     * Variable: length
     *  Instances of the data type.
     *
     *  When used as a base-type, i.e not an array; this will be
     *  1, otherwise it will be the length of the array, or -1
     *  if the size of the array is unknown.
     */
    int length;

    /*
     * Variable: pointer
     *  Pointer to pointer type if pointer
     *
     * When the variable is a pointer type, this will point to another
     * data type that describes the base type of the pointer, NULL other-
     * wise.
     */
    data_type_t *pointer;

    /* structure */
    struct {
        /*
         * Variable: fields
         *  Pointer to a table of fields (if structure)
         */
        table_t *fields;

        /*
         * Variable: offset
         *  Offset of the given field in a structure (if a structure base type)
         */
        int offset;
    };

    /* function */
    struct {
        /*
         * Variable: returntype
         *  Pointer to a data type which describes the return type
         *  of the function (if a function)
         */
        data_type_t *returntype;

        /*
         * Variable: parameters
         *  Pointer to a list of parameters for a function.
         */
        list_t *parameters;

        /*
         * Variable: hasdots
         *  Describes if the given function is variable-argument.
         *
         *  Contains the value `true` when the function has
         *  three dots `...` in it's prototype, otherwise `false`.
         */
        bool hasdots;
    };
};

/*
 * Class: ast_string_t
 *  The *AST_TYPE_STRING* ast node.
 */
typedef struct {
    /*
     * Variable: data
     *  String contents
     */
    char *data;

    /*
     * Variable: label
     *  Name of the label associated with the string.
     */
    char *label;
} ast_string_t;

/*
 * Class: ast_variable_t
 *  The *AST_TYPE_VAR_LOCAL* and *AST_TYPE_VAR_GLOBAL* ast node.
 */
typedef struct {
    /*
     * Variable: name
     *  Name of the variable
     */
    char *name;

    /*
     * Variable: off
     *  Offset of the variable on the stack.
     */
    int off;

    /*
     * Variable:
     *  Name of the label associated with the variable.
     */
    char *label;
} ast_variable_t;

/*
 * Class ast_function_call_t
 *  Function call
 *
 *  Remarks:
 *      Not associated with any node. Instead describes the
 *      data associated with a function call for *ast_function_t*
 */
typedef struct {
    /*
     * Variable: args
     *  Pointer to a list of arguments for a function call
     */
    list_t *args;

    /*
     * Variable: paramtypes
     *  Pointer to a list of parameter types for the function call.
     */
    list_t *paramtypes;
} ast_function_call_t;

/*
 * Class: ast_function_t
 *  The *AST_TYPE_FUNCTION* ast node.
 */
typedef struct {
    /*
     * Variable: name
     *  The function name
     */
    char *name;

    /*
     * Variable: call
     *  Data associated with a function call.
     */
    ast_function_call_t call;

    /*
     * Variable: params
     *  Pointer to a list of parameters.
     */
    list_t *params;

    /*
     * Variable: locals
     *  Pointer to a list of locals.
     */
    list_t *locals;

    /*
     * Variable: body
     *  Pointer to an ast node which describes the body.
     *
     * Remarks:
     *  A body is usually composed of a serise of ast nodes,
     *  typically a compound expression, but could also contain
     *  nested compound expressions. Think of this as a pointer
     *  to the head of the beginning of a serise of basic-blocks
     *  which are the forming of the function body.
     */
    ast_t  *body;
} ast_function_t;

/*
 * Class: ast_unary_t
 *  Represents a unary operation in the AST tree
 */
typedef struct {
    /*
     * Variable: operand
     *  Pointer to the operand the unary operation is to
     *  be performed on.
     */
    ast_t *operand;
} ast_unary_t;

/*
 * Class: ast_decl_t
 *  Represents a declaration in the AST tree
 */
typedef struct {
    /*
     * Variable: var
     *  Pointer to the variable node associated with the
     *  declaration.
     */
    ast_t *var;

    /*
     * Variable: init
     *  When the declaration includes an initialization this points
     *  to such a node, otherwise it points to NULL.
     */
    ast_t *init;
} ast_decl_t;

/*
 * Class: ast_ifthan_t
 *  Represents a if-than node in the AST tree.
 *
 * Remarks:
 *  Describes a two-branch gaurded by conditional test node in the AST
 *  tree for implementing ternary expressions and if statements.
 */
typedef struct {
    /*
     * Variable: cond
     *  The condition node
     */
    ast_t  *cond;

    /*
     * Variable: then
     *  Basic block for truth path in branch
     */
    ast_t  *then;

    /*
     * Variable: last
     *  Basic block for false path in branch
     */
    ast_t  *last;
} ast_ifthan_t;

/*
 * Class: ast_for_t
 *  Represents a for-loop node in the AST tree.
 *
 * Remarks:
 *  Standard for loop with precondition / initilization
 *  expression, conditionally testsed, and post step /
 *  expression, ergo for(init; cond; step) body;
 */
typedef struct {
    /* Variable: init */
    ast_t  *init;
    /* Variable: cond */
    ast_t  *cond;
    /* Variable: step */
    ast_t  *step;
    /* Variable: body */
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

    union {
        int             casevalue;
        long            integer;
        char            character;
        ast_string_t    string;
        ast_variable_t  variable;
        ast_function_t  function;
        ast_unary_t     unary;
        ast_decl_t      decl;
        ast_ifthan_t    ifstmt;
        ast_for_t       forstmt;
        ast_switch_t    switchstmt;
        ast_t          *returnstmt;
        list_t         *compound;
        ast_initlist_t  initlist;
        ast_goto_t      gotostmt;

        struct {
            ast_t *left;
            ast_t *right;
        };

        struct {
            ast_t       *structure;
            char        *field;
            data_type_t *fieldtype;
        };

        struct {
            double value;
            char  *label;
        } floating;

    };
};

/*
 * Function: ast_structure_reference
 *  Creates an structure reference of a given type for a given field
 *
 * Parameters:
 *  type      - The type of the field for reference
 *  structure - The structure that contains said field to be referenced
 *  name      - The name of the field in that structure to reference
 *
 * Returns:
 *  An ast node referencing that field in that paticular structure on
 *  success, otherwise NULL.
 */
ast_t *ast_structure_reference(data_type_t *type, ast_t *structure, char *name);

/*
 * Function: ast_structure_field
 *  Copies a given field data type and changes it offset
 *
 * Parameters:
 *  type   - Pointer to the structure field data type
 *  offset - The offset of the copied data type in the structure
 *
 * Returns:
 *  A copy of the structure fields data type with the supplied
 *  offset on success, NULL otherwise.
 */
data_type_t *ast_structure_field(data_type_t *type, int offset);

/*
 * Function: ast_structure_new
 *  Creates a structure data type
 *
 * Parameters;
 *  field   - A table of data_type_t fields for the structure
 *  size    - The size of the structure
 *
 * Returns:
 *  A new structure data type with the specified fields and size on
 *  success, NULL otherwise.
 */
data_type_t *ast_structure_new(table_t *fields, int size);


ast_t *ast_new_unary(int type, data_type_t *data, ast_t *operand);
ast_t *ast_new_binary(int type, ast_t *left, ast_t *right);
ast_t *ast_new_integer(data_type_t *type, int value);
ast_t *ast_new_floating(data_type_t *, double value);
ast_t *ast_new_char(char value);
ast_t *ast_new_string(char *value);
ast_t *ast_new_label(char *);

char *ast_label(void);

ast_t *ast_declaration(ast_t *var, ast_t *init);
ast_t *ast_variable_local(data_type_t *type, char *name);
ast_t *ast_variable_global(data_type_t *type, char *name);
ast_t *ast_call(data_type_t *type, char *name, list_t *args, list_t *paramtypes);
ast_t *ast_function(data_type_t *type, char *name, list_t *params, ast_t *body, list_t *locals);
ast_t *ast_declaration(ast_t *var, ast_t *init);
ast_t *ast_initializerlist(list_t *init);
ast_t *ast_if(ast_t *cond, ast_t *then, ast_t *last);
ast_t *ast_for(ast_t *init, ast_t *cond, ast_t *step, ast_t *body);
ast_t *ast_while(ast_t *cond, ast_t *body);
ast_t *ast_do(ast_t *cond, ast_t *body);
ast_t *ast_return(data_type_t *returntype, ast_t *val);
ast_t *ast_compound(list_t *statements);
ast_t *ast_ternary(data_type_t *type, ast_t *cond, ast_t *then, ast_t *last);
ast_t *ast_switch(ast_t *expr, ast_t *body);
ast_t *ast_case(int value);
ast_t *ast_goto(char *);
ast_t *ast_make(int type);

data_type_t *ast_prototype(data_type_t *returntype, list_t *paramtypes, bool dots);
data_type_t *ast_pointer(data_type_t *type);
data_type_t *ast_array(data_type_t *type, int size);
data_type_t *ast_array_convert(data_type_t *ast);
data_type_t *ast_result_type(char op, data_type_t *a, data_type_t *b);

const char *ast_type_string(data_type_t *type);
bool ast_type_integer(data_type_t *type);
bool ast_type_floating(data_type_t *type);
data_type_t *ast_type_copy(data_type_t *type);
data_type_t *ast_type_create(type_t type, bool sign);
data_type_t *ast_type_stub(void);


extern data_type_t *ast_data_table[AST_DATA_COUNT];

extern list_t      *ast_floats;
extern list_t      *ast_strings;
extern list_t      *ast_locals;
extern list_t      *ast_gotos;
extern table_t     *ast_globalenv;
extern table_t     *ast_localenv;
extern table_t     *ast_structures;
extern table_t     *ast_unions;
extern table_t     *ast_labels;

char *ast_string(ast_t *ast);

#endif
