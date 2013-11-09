#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "lice.h"
#include "ast.h"
#include "lexer.h"

data_type_t *ast_data_table[AST_DATA_COUNT] = {
    &(data_type_t) { TYPE_VOID,    0,                      true },   /* void                */
    &(data_type_t) { TYPE_LONG,    ARCH_TYPE_SIZE_LONG,    true },   /* long                */
    &(data_type_t) { TYPE_LLONG,   ARCH_TYPE_SIZE_LLONG,   true },   /* long long           */
    &(data_type_t) { TYPE_INT,     ARCH_TYPE_SIZE_INT,     true },   /* int                 */
    &(data_type_t) { TYPE_SHORT,   ARCH_TYPE_SIZE_SHORT,   true },   /* short               */
    &(data_type_t) { TYPE_CHAR,    ARCH_TYPE_SIZE_CHAR,    true },   /* char                */
    &(data_type_t) { TYPE_FLOAT,   ARCH_TYPE_SIZE_FLOAT,   true },   /* float               */
    &(data_type_t) { TYPE_DOUBLE,  ARCH_TYPE_SIZE_DOUBLE,  true },   /* double              */
    &(data_type_t) { TYPE_LDOUBLE, ARCH_TYPE_SIZE_LDOUBLE, true },   /* long double         */
    &(data_type_t) { TYPE_LONG,    ARCH_TYPE_SIZE_LONG,    false },  /* unsigned long       */
    &(data_type_t) { TYPE_LLONG,   ARCH_TYPE_SIZE_LLONG,   false },  /* unsigned long long  */
    NULL                                                             /* function            */
};

data_type_t *ast_data_function = NULL;

list_t      *ast_locals      = NULL;
list_t      *ast_gotos       = NULL;
list_t      *ast_floats      = &SENTINEL_LIST;
list_t      *ast_strings     = &SENTINEL_LIST;

table_t     *ast_labels      = NULL;
table_t     *ast_globalenv   = &SENTINEL_TABLE;
table_t     *ast_localenv    = &SENTINEL_TABLE;
table_t     *ast_structures  = &SENTINEL_TABLE;
table_t     *ast_unions      = &SENTINEL_TABLE;

static data_type_t *ast_result_type_impl(jmp_buf *jmpbuf, char op, data_type_t *a, data_type_t *b) {
    if (a->type > b->type) {
        data_type_t *t = a;
        a = b;
        b = t;
    }

    if (b->type == TYPE_POINTER) {
        if (op == '=')
            return a;
        if (op != '+' && op != '-')
            goto error;
        if (!ast_type_integer(a))
            goto error;

        return b;
    }

    switch (a->type) {
        case TYPE_VOID:
            goto error;
        case TYPE_CHAR:
        case TYPE_SHORT:
        case TYPE_INT:
            switch (b->type) {
                case TYPE_CHAR:
                case TYPE_SHORT:
                case TYPE_INT:
                    return ast_data_table[AST_DATA_INT];
                case TYPE_LONG:
                case TYPE_LLONG:
                    return ast_data_table[AST_DATA_LONG];
                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                case TYPE_LDOUBLE:
                    return ast_data_table[AST_DATA_DOUBLE];
                case TYPE_ARRAY:
                case TYPE_POINTER:
                    return b;
                default:
                    break;
            }
            compile_error("Internal error: ast_result_type %d", b->type);

        case TYPE_LONG:
        case TYPE_LLONG:
            switch (b->type) {
                case TYPE_LONG:
                case TYPE_LLONG:
                    return ast_data_table[AST_DATA_LONG];
                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                case TYPE_LDOUBLE:
                    return ast_data_table[AST_DATA_DOUBLE];
                case TYPE_ARRAY:
                case TYPE_POINTER:
                    return b;
                default:
                    break;
            }
            compile_error("Internal error: ast_result_type (3)");

        case TYPE_FLOAT:
            if (b->type == TYPE_FLOAT || b->type == TYPE_DOUBLE || b->type == TYPE_LDOUBLE)
                return ast_data_table[AST_DATA_DOUBLE];
            goto error;

        case TYPE_DOUBLE:
        case TYPE_LDOUBLE:
            if (b->type == TYPE_DOUBLE || b->type == TYPE_LDOUBLE)
                return ast_data_table[AST_DATA_DOUBLE];
            goto error;

        case TYPE_ARRAY:
            if (b->type != TYPE_ARRAY)
                goto error;
            return ast_result_type_impl(jmpbuf, op, a->pointer, b->pointer);
        default:
            compile_error("ICE");
    }

error:
    longjmp(*jmpbuf, 1);
    return NULL;
}

data_type_t *ast_result_type(int op, data_type_t *a, data_type_t *b) {

    switch (op) {
        case '!':
        case '~':
        case '<':
        case '>':
        case '&':
        case '%':
        case AST_TYPE_EQUAL:
        case AST_TYPE_GEQUAL:
        case AST_TYPE_LEQUAL:
        case AST_TYPE_NEQUAL:
        case AST_TYPE_LSHIFT:
        case AST_TYPE_RSHIFT:
        case AST_TYPE_AND:
        case AST_TYPE_OR:
            return ast_data_table[AST_DATA_INT];
    }

    jmp_buf jmpbuf;
    if (setjmp(jmpbuf) == 0) {
        return ast_result_type_impl(
                    &jmpbuf,
                    op,
                    ast_array_convert(a),
                    ast_array_convert(b)
        );
    }

    compile_error(
        "incompatible operands `%s' and `%s' in `%c` operation",
        ast_type_string(a),
        ast_type_string(b),
        op
    );

    return NULL;
}

ast_t *ast_copy(ast_t *ast) {
    ast_t *copy = memory_allocate(sizeof(ast_t));
    *copy = *ast;
    return copy;
}

ast_t *ast_structure_reference(data_type_t *type, ast_t *structure, char *name) {
    return ast_copy(&(ast_t) {
        .type       = AST_TYPE_STRUCT,
        .ctype      = type,
        .structure  = structure,
        .field      = name
    });
}

ast_t *ast_new_unary(int type, data_type_t *data, ast_t *operand) {
    return ast_copy(&(ast_t) {
        .type          = type,
        .ctype         = data,
        .unary.operand = operand
    });
}

ast_t *ast_new_binary(int type, ast_t *left, ast_t *right) {
    ast_t *ast = ast_copy(&(ast_t){
        .type  = type,
        .ctype = ast_result_type(type, left->ctype, right->ctype),
    });
    if (type != '='
        && ast_array_convert(left->ctype)->type  != TYPE_POINTER
        && ast_array_convert(right->ctype)->type == TYPE_POINTER) {

        ast->left  = right;
        ast->right = left;
    } else {
        ast->left  = left;
        ast->right = right;
    }
    return ast;
}

ast_t *ast_new_integer(data_type_t *type, int value) {
    return ast_copy(&(ast_t) {
        .type    = AST_TYPE_LITERAL,
        .ctype   = type,
        .integer = value
    });
}

ast_t *ast_new_floating(data_type_t *type, double value) {
    ast_t *ast = ast_copy(&(ast_t){
        .type           = AST_TYPE_LITERAL,
        .ctype          = type,
        .floating.value = value
    });
    list_push(ast_floats, ast);
    return ast;
}

ast_t *ast_new_string(char *value) {
    return ast_copy(&(ast_t) {
        .type         = AST_TYPE_STRING,
        .ctype        = ast_array(ast_data_table[AST_DATA_CHAR], strlen(value) + 1),
        .string.data  = value,
        .string.label = ast_label()
    });
}

ast_t *ast_variable_local(data_type_t *type, char *name) {
    ast_t *ast = ast_copy(&(ast_t){
        .type          = AST_TYPE_VAR_LOCAL,
        .ctype         = type,
        .variable.name = name
    });
    if (ast_localenv)
        table_insert(ast_localenv, name, ast);
    if (ast_locals)
        list_push(ast_locals, ast);
    return ast;
}

ast_t *ast_variable_global(data_type_t *type, char *name) {
    ast_t *ast = ast_copy(&(ast_t){
        .type           = AST_TYPE_VAR_GLOBAL,
        .ctype          = type,
        .variable.name  = name,
        .variable.label = name
    });
    table_insert(ast_globalenv, name, ast);
    return ast;
}

ast_t *ast_call(data_type_t *type, char *name, list_t *arguments, list_t *parametertypes) {
    return ast_copy(&(ast_t) {
        .type                     = AST_TYPE_CALL,
        .ctype                    = type,
        .function.call.paramtypes = parametertypes,
        .function.call.args       = arguments,
        .function.name            = name
    });
}

ast_t *ast_function(data_type_t *ret, char *name, list_t *params, ast_t *body, list_t *locals) {
    return ast_copy(&(ast_t) {
        .type            = AST_TYPE_FUNCTION,
        .ctype           = ret,
        .function.name   = name,
        .function.params = params,
        .function.locals = locals,
        .function.body   = body
    });
}

ast_t *ast_declaration(ast_t *var, list_t *init) {
    return ast_copy(&(ast_t) {
        .type      = AST_TYPE_DECLARATION,
        .ctype     = NULL,
        .decl.var  = var,
        .decl.init = init,
    });
}

ast_t *ast_initializer(ast_t *value, data_type_t *to, int offset) {
    return ast_copy(&(ast_t){
        .type          = AST_TYPE_INITIALIZER,
        .init.value    = value,
        .init.offset   = offset,
        .init.type     = to
    });
}

ast_t *ast_ternary(data_type_t *type, ast_t *cond, ast_t *then, ast_t *last) {
    return ast_copy(&(ast_t){
        .type         = AST_TYPE_EXPRESSION_TERNARY,
        .ctype        = type,
        .ifstmt.cond  = cond,
        .ifstmt.then  = then,
        .ifstmt.last  = last
    });
}

static ast_t *ast_for_intermediate(int type, ast_t *init, ast_t *cond, ast_t *step, ast_t *body) {
    return ast_copy(&(ast_t){
        .type         = type,
        .ctype        = NULL,
        .forstmt.init = init,
        .forstmt.cond = cond,
        .forstmt.step = step,
        .forstmt.body = body
    });
}

ast_t *ast_switch(ast_t *expr, ast_t *body) {
    return ast_copy(&(ast_t){
        .type            = AST_TYPE_STATEMENT_SWITCH,
        .switchstmt.expr = expr,
        .switchstmt.body = body
    });
}

ast_t *ast_case(int value) {
    return ast_copy(&(ast_t){
        .type      = AST_TYPE_STATEMENT_CASE,
        .casevalue = value
    });
}

ast_t *ast_make(int type) {
    return ast_copy(&(ast_t){
        .type = type
    });
}

ast_t *ast_if(ast_t *cond, ast_t *then, ast_t *last) {
    return ast_copy(&(ast_t){
        .type        = AST_TYPE_STATEMENT_IF,
        .ctype       = NULL,
        .ifstmt.cond = cond,
        .ifstmt.then = then,
        .ifstmt.last = last
    });
}

ast_t *ast_for(ast_t *init, ast_t *cond, ast_t *step, ast_t *body) {
    return ast_for_intermediate(AST_TYPE_STATEMENT_FOR, init, cond, step, body);
}
ast_t *ast_while(ast_t *cond, ast_t *body) {
    return ast_for_intermediate(AST_TYPE_STATEMENT_WHILE, NULL, cond, NULL, body);
}
ast_t *ast_do(ast_t *cond, ast_t *body) {
    return ast_for_intermediate(AST_TYPE_STATEMENT_DO, NULL, cond, NULL, body);
}

ast_t *ast_goto(char *label) {
    return ast_copy(&(ast_t){
        .type           = AST_TYPE_STATEMENT_GOTO,
        .gotostmt.label = label,
        .gotostmt.where = NULL
    });
}

ast_t *ast_new_label(char *label) {
    return ast_copy(&(ast_t){
        .type           = AST_TYPE_STATEMENT_LABEL,
        .gotostmt.label = label,
        .gotostmt.where = NULL
    });
}

ast_t *ast_return(data_type_t *returntype, ast_t *value) {
    return ast_copy(&(ast_t){
        .type       = AST_TYPE_STATEMENT_RETURN,
        .ctype      = returntype,
        .returnstmt = value
    });
}

ast_t *ast_compound(list_t *statements) {
    return ast_copy(&(ast_t){
        .type     = AST_TYPE_STATEMENT_COMPOUND,
        .ctype    = NULL,
        .compound = statements
    });
}

data_type_t *ast_structure_field(data_type_t *type, int offset) {
    data_type_t *field = ast_type_copy(type);
    field->offset = offset;
    return field;
}

data_type_t *ast_structure_new(table_t *fields, int size, bool isstruct) {
    return ast_type_copy(&(data_type_t) {
        .type     = TYPE_STRUCTURE,
        .size     = size,
        .fields   = fields,
        .isstruct = isstruct
    });
}

char *ast_label(void) {
    static int index = 0;
    string_t *string = string_create();
    string_catf(string, ".L%d", index++);
    return string_buffer(string);
}

bool ast_type_integer(data_type_t *type) {
    return type->type == TYPE_CHAR
        || type->type == TYPE_SHORT
        || type->type == TYPE_INT
        || type->type == TYPE_LONG
        || type->type == TYPE_LLONG;
}

bool ast_type_floating(data_type_t *type) {
    return type->type == TYPE_FLOAT
        || type->type == TYPE_DOUBLE
        || type->type == TYPE_LDOUBLE;
}

data_type_t *ast_type_copy(data_type_t *type) {
    return memcpy(memory_allocate(sizeof(data_type_t)), type, sizeof(data_type_t));
}

data_type_t *ast_type_copy_incomplete(data_type_t *type) {
    if (!type)
        return NULL;
    return (type->length == -1)
                ? ast_type_copy(type)
                : type;
}

data_type_t *ast_type_create(type_t type, bool sign) {

    data_type_t *t = memory_allocate(sizeof(data_type_t));

    t->type = type;
    t->sign = sign;

    switch (type) {
        case TYPE_VOID:    t->size = 0;                      break;
        case TYPE_CHAR:    t->size = ARCH_TYPE_SIZE_CHAR;    break;
        case TYPE_SHORT:   t->size = ARCH_TYPE_SIZE_SHORT;   break;
        case TYPE_INT:     t->size = ARCH_TYPE_SIZE_INT;     break;
        case TYPE_LONG:    t->size = ARCH_TYPE_SIZE_LONG;    break;
        case TYPE_LLONG:   t->size = ARCH_TYPE_SIZE_LLONG;   break;
        case TYPE_FLOAT:   t->size = ARCH_TYPE_SIZE_FLOAT;   break;
        case TYPE_DOUBLE:  t->size = ARCH_TYPE_SIZE_DOUBLE;  break;
        case TYPE_LDOUBLE: t->size = ARCH_TYPE_SIZE_LDOUBLE; break;
        default:
            compile_error("ICE");
    }

    return t;
}

data_type_t *ast_type_stub(void) {
    return ast_type_copy(&(data_type_t) {
        .type = TYPE_CDECL,
        .size = 0
    });
}

data_type_t *ast_prototype(data_type_t *returntype, list_t *paramtypes, bool dots) {
    return ast_type_copy(&(data_type_t){
        .type       = TYPE_FUNCTION,
        .returntype = returntype,
        .parameters = paramtypes,
        .hasdots    = dots
    });
}

data_type_t *ast_array(data_type_t *type, int length) {
    return ast_type_copy(&(data_type_t){
        .type    = TYPE_ARRAY,
        .pointer = type,
        .size    = (length < 0) ? -1 : type->size * length,
        .length  = length
    });
}

data_type_t *ast_array_convert(data_type_t *type) {
    if (type->type != TYPE_ARRAY)
        return type;
    return ast_pointer(type->pointer);
}

data_type_t *ast_pointer(data_type_t *type) {
    return ast_type_copy(&(data_type_t){
        .type    = TYPE_POINTER,
        .pointer = type,
        .size    = ARCH_TYPE_SIZE_POINTER
    });
}

const char *ast_type_string(data_type_t *type) {
    string_t *string;

    switch (type->type) {
        case TYPE_VOID:     return "void";
        case TYPE_INT:      return "int";
        case TYPE_CHAR:     return "char";
        case TYPE_LONG:     return "long";
        case TYPE_LLONG:    return "long long";
        case TYPE_SHORT:    return "short";
        case TYPE_FLOAT:    return "float";
        case TYPE_DOUBLE:   return "double";
        case TYPE_LDOUBLE:  return "long double";

        case TYPE_FUNCTION:
            string = string_create();
            string_cat(string, '(');
            for (list_iterator_t *it = list_iterator(type->parameters); !list_iterator_end(it); ) {
                data_type_t *next = list_iterator_next(it);
                string_catf(string, "%s", ast_type_string(next));
                if (!list_iterator_end(it))
                    string_cat(string, ',');
            }
            string_catf(string, ") -> %s", ast_type_string(type->returntype));
            return string_buffer(string);

        case TYPE_POINTER:
            string = string_create();
            string_catf(string, "%s*", ast_type_string(type->pointer));
            return string_buffer(string);

        case TYPE_ARRAY:
            string = string_create();
            string_catf(
                string,
                "%s[%d]",
                ast_type_string(type->pointer),
                type->length
            );
            return string_buffer(string);

        case TYPE_STRUCTURE:
            string = string_create();
            string_catf(string, "(struct");
            for (list_iterator_t *it = list_iterator(table_values(type->fields)); !list_iterator_end(it); )
                string_catf(string, " (%s)", ast_type_string(list_iterator_next(it)));
            string_cat(string, ')');
            return string_buffer(string);

        default:
            break;
    }
    return NULL;
}

static void ast_string_unary(string_t *string, const char *op, ast_t *ast) {
    string_catf(string, "(%s %s)", op, ast_string(ast->unary.operand));
}

static void ast_string_binary(string_t *string, const char *op, ast_t *ast) {
    string_catf(string, "(%s %s %s)", op, ast_string(ast->left), ast_string(ast->right));
}

static void ast_string_initialization_declaration(string_t *string, list_t *initlist) {
    for (list_iterator_t *it = list_iterator(initlist); !list_iterator_end(it); ) {
        ast_t *init = list_iterator_next(it);
        string_catf(string, "%s", ast_string(init));
        if (!list_iterator_end(it))
            string_cat(string, ' ');
    }
}

static void ast_string_impl(string_t *string, ast_t *ast) {
    char *left;
    char *right;

    if (!ast) {
        string_catf(string, "(null)");
        return;
    }

    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_INT:
                case TYPE_SHORT:
                    string_catf(string, "%d",   ast->integer);
                    break;

                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                    string_catf(string, "%f",   ast->floating.value);
                    break;

                case TYPE_LONG:
                    string_catf(string, "%ldL", ast->integer);
                    break;

                case TYPE_CHAR:
                    if (ast->integer == '\n')
                        string_catf(string, "'\n'");
                    else if (ast->integer == '\\')
                        string_catf(string, "'\\\\'");
                    else if (ast->integer == '\0')
                        string_catf(string, "'\\0'");
                    else
                        string_catf(string, "'%c'", ast->integer);
                    break;

                default:
                    compile_error("Internal error: ast_string_impl");
                    break;
            }
            break;

        case AST_TYPE_STRING:
            string_catf(string, "\"%s\"", string_quote(ast->string.data));
            break;

        case AST_TYPE_VAR_LOCAL:
            string_catf(string, "%s", ast->variable.name);
            if (ast->variable.init) {
                string_cat(string, '(');
                ast_string_initialization_declaration(string, ast->variable.init);
                string_cat(string, ')');
            }
            break;

        case AST_TYPE_VAR_GLOBAL:
            string_catf(string, "%s", ast->variable.name);
            break;

        case AST_TYPE_CALL:
            string_catf(string, "(%s)%s(", ast_type_string(ast->ctype), ast->function.name);
            for (list_iterator_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                string_catf(string, "%s", ast_string(list_iterator_next(it)));
                if (!list_iterator_end(it))
                    string_cat(string, ',');
            }
            string_cat(string, ')');
            break;

        case AST_TYPE_FUNCTION:
            string_catf(string, "(%s)%s(", ast_type_string(ast->ctype), ast->function.name);
            for (list_iterator_t *it = list_iterator(ast->function.params); !list_iterator_end(it); ) {
                ast_t *param = list_iterator_next(it);
                string_catf(string, "%s %s", ast_type_string(param->ctype), ast_string(param));
                if (!list_iterator_end(it))
                    string_cat(string, ',');
            }
            string_cat(string, ')');
            ast_string_impl(string, ast->function.body);
            break;

        case AST_TYPE_DECLARATION:
            string_catf(string, "(decl %s %s",
                    ast_type_string(ast->decl.var->ctype),
                    ast->decl.var->variable.name
            );
            ast_string_initialization_declaration(string, ast->decl.init);
            string_cat(string, ')');
            break;

        case AST_TYPE_INITIALIZER:
            string_catf(string, "%s@%d", ast_string(ast->init.value), ast->init.offset);
            break;

        case AST_TYPE_STATEMENT_COMPOUND:
            string_cat(string, '{');
            for (list_iterator_t *it = list_iterator(ast->compound); !list_iterator_end(it); ) {
                ast_string_impl(string, list_iterator_next(it));
                string_cat(string, ';');
            }
            string_cat(string, '}');
            break;

        case AST_TYPE_STRUCT:
            ast_string_impl(string, ast->structure);
            string_cat(string, '.');
            string_catf(string, ast->field);
            break;

        case AST_TYPE_EXPRESSION_TERNARY:
            string_catf(string, "(? %s %s %s)",
                            ast_string(ast->ifstmt.cond),
                            ast_string(ast->ifstmt.then),
                            ast_string(ast->ifstmt.last)
            );
            break;

        case AST_TYPE_STATEMENT_IF:
            string_catf(string, "(if %s %s", ast_string(ast->ifstmt.cond), ast_string(ast->ifstmt.then));
            if (ast->ifstmt.last)
                string_catf(string, " %s", ast_string(ast->ifstmt.last));
            string_cat(string, ')');
            break;

        case AST_TYPE_STATEMENT_FOR:
            string_catf(string, "(for %s %s %s %s)",
                ast_string(ast->forstmt.init),
                ast_string(ast->forstmt.cond),
                ast_string(ast->forstmt.step),
                ast_string(ast->forstmt.body)
            );
            break;

        case AST_TYPE_STATEMENT_WHILE:
            string_catf(string, "(while %s %s)",
                ast_string(ast->forstmt.cond),
                ast_string(ast->forstmt.body)
            );
            break;

        case AST_TYPE_STATEMENT_DO:
            string_catf(string, "(do %s %s)",
                ast_string(ast->forstmt.cond),
                ast_string(ast->forstmt.body)
            );
            break;

        case AST_TYPE_STATEMENT_RETURN:
            string_catf(string, "(return %s)", ast_string(ast->returnstmt));
            break;

        case AST_TYPE_ADDRESS:      ast_string_unary (string, "&",  ast); break;
        case AST_TYPE_DEREFERENCE:  ast_string_unary (string, "*",  ast); break;
        case LEXER_TOKEN_INCREMENT: ast_string_unary (string, "++", ast); break;
        case LEXER_TOKEN_DECREMENT: ast_string_unary (string, "--", ast); break;
        case '!':                   ast_string_unary (string, "!",  ast); break;
        case '&':                   ast_string_binary(string, "&",  ast); break;
        case '|':                   ast_string_binary(string, "|",  ast); break;
        case LEXER_TOKEN_AND:       ast_string_binary(string, "&&", ast); break;
        case LEXER_TOKEN_OR:        ast_string_binary(string, "||", ast); break;
        case LEXER_TOKEN_GEQUAL:    ast_string_binary(string, ">=", ast); break;
        case LEXER_TOKEN_LEQUAL:    ast_string_binary(string, "<=", ast); break;
        case LEXER_TOKEN_NEQUAL:    ast_string_binary(string, "!=", ast); break;

        case AST_TYPE_EXPRESSION_CAST:
            string_catf(string, "((%s) -> (%s) %s)",
                ast_type_string(ast->unary.operand->ctype),
                ast_type_string(ast->ctype),
                ast_string(ast->unary.operand)
            );
            break;

        default:
            left  = ast_string(ast->left);
            right = ast_string(ast->right);
            if (ast->type == LEXER_TOKEN_EQUAL)
                string_catf(string, "(== %s %s)", left, right);
            else
                string_catf(string, "(%c %s %s)", ast->type, left, right);
            break;
    }
}

char *ast_string(ast_t *ast) {
    string_t *string = string_create();
    ast_string_impl(string, ast);
    return string_buffer(string);
}
