#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "lice.h"

// todo remove
static int ast_label_index = 0;

data_type_t *ast_data_void     = &(data_type_t) { TYPE_VOID,      0, true  };
data_type_t *ast_data_long     = &(data_type_t) { TYPE_LONG,      8, true  };
data_type_t *ast_data_int      = &(data_type_t) { TYPE_INT,       4, true  };
data_type_t *ast_data_short    = &(data_type_t) { TYPE_SHORT,     2, true  };
data_type_t *ast_data_char     = &(data_type_t) { TYPE_CHAR,      1, true  };
data_type_t *ast_data_ulong    = &(data_type_t) { TYPE_LONG,      8, false };
data_type_t *ast_data_float    = &(data_type_t) { TYPE_FLOAT,     4, true  };
data_type_t *ast_data_double   = &(data_type_t) { TYPE_DOUBLE,    8, true  };
data_type_t *ast_data_function = NULL;

list_t      *ast_locals      = NULL;
list_t      *ast_floats      = &SENTINEL_LIST;
list_t      *ast_strings     = &SENTINEL_LIST;
table_t     *ast_globalenv   = &SENTINEL_TABLE;
table_t     *ast_localenv    = &SENTINEL_TABLE;
table_t     *ast_structures  = &SENTINEL_TABLE;
table_t     *ast_unions      = &SENTINEL_TABLE;


// ast result
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
        case TYPE_INT:
        case TYPE_CHAR:
        case TYPE_SHORT:
            switch (b->type) {
                case TYPE_INT:
                case TYPE_CHAR:
                case TYPE_SHORT:
                    return ast_data_int;
                case TYPE_LONG:
                case TYPE_LLONG:
                    return ast_data_long;
                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                case TYPE_LDOUBLE:
                    return ast_data_double;
                case TYPE_ARRAY:
                case TYPE_POINTER:
                    return b;
                default:
                    break;
            }
            compile_error("Internal error: ast_result_type (1)");

        // outside for the future 'long long' type
        case TYPE_LONG:
        case TYPE_LLONG:
            switch (b->type) {
                case TYPE_LONG:
                case TYPE_LLONG:
                    return ast_data_long;
                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                case TYPE_LDOUBLE:
                    return ast_data_double;
                case TYPE_ARRAY:
                case TYPE_POINTER:
                    return b;
                default:
                    break;
            }
            compile_error("Internal error: ast_result_type (3)");

        case TYPE_FLOAT:
            if (b->type == TYPE_FLOAT || b->type == TYPE_DOUBLE || b->type == TYPE_LDOUBLE)
                return ast_data_double;
            goto error;

        case TYPE_DOUBLE:
        case TYPE_LDOUBLE:
            if (b->type == TYPE_DOUBLE || b->type == TYPE_LDOUBLE)
                return ast_data_double;
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

data_type_t *ast_result_type(char op, data_type_t *a, data_type_t *b) {
    jmp_buf jmpbuf;
    if (setjmp(jmpbuf) == 0) {
        return ast_result_type_impl(
                    &jmpbuf,
                    op,
                    ast_array_convert(a),
                    ast_array_convert(b)
        );
    }
    compile_error("Incompatible types in expression");
    return NULL;
}

#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))


////////////////////////////////////////////////////////////////////////
// structures
ast_t *ast_structure_reference_new(data_type_t *type, ast_t *structure, char *name) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_STRUCT;
    ast->ctype     = type;
    ast->structure = structure;
    ast->field     = name;

    return ast;
}

data_type_t *ast_structure_field_new(data_type_t *type, int offset) {
    data_type_t *field = ast_type_copy(type);
    field->offset = offset;
    return field;
}

data_type_t *ast_structure_new(table_t *fields, int size) {
    data_type_t *structure = (data_type_t*)malloc(sizeof(data_type_t));
    structure->type        = TYPE_STRUCTURE;
    structure->size        = size;
    structure->fields      = fields;

    return structure;
}

////////////////////////////////////////////////////////////////////////
// unary and binary
ast_t *ast_new_unary(int type, data_type_t *data, ast_t *operand) {
    ast_t *ast         = ast_new_node();
    ast->type          = type;
    ast->ctype         = data;
    ast->unary.operand = operand;

    return ast;
}

ast_t *ast_new_binary(int type, ast_t *left, ast_t *right) {
    ast_t *ast         = ast_new_node();
    ast->type          = type;
    ast->ctype         = ast_result_type(type, left->ctype, right->ctype);
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

////////////////////////////////////////////////////////////////////////
// data types
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
    data_type_t *t = malloc(sizeof(data_type_t));
    memcpy(t, type, sizeof(data_type_t));
    return t;
}

data_type_t *ast_type_create(type_t type, bool sign) {
    data_type_t *t = malloc(sizeof(data_type_t));

    t->type = type;
    t->sign = sign;

    switch (type) {
        case TYPE_VOID:    t->size = 0; break;
        case TYPE_CHAR:    t->size = 1; break;
        case TYPE_SHORT:   t->size = 2; break;
        case TYPE_INT:     t->size = 4; break;
        case TYPE_LONG:    t->size = 8; break;
        case TYPE_LLONG:   t->size = 8; break;
        case TYPE_FLOAT:   t->size = 8; break;
        case TYPE_DOUBLE:  t->size = 8; break;
        case TYPE_LDOUBLE: t->size = 8; break;
        default:
            compile_error("ICE");
    }

    return t;
}

ast_t *ast_new_integer(data_type_t *type, int value) {
    ast_t *ast   = ast_new_node();
    ast->type    = AST_TYPE_LITERAL;
    ast->ctype   = type;
    ast->integer = value;

    return ast;
}

ast_t *ast_new_floating(double value) {
    ast_t *ast          = ast_new_node();
    ast->type           = AST_TYPE_LITERAL;
    ast->ctype          = ast_data_double;
    ast->floating.value = value;
    list_push(ast_floats, ast);
    return ast;
}

ast_t *ast_new_string(char *value) {
    ast_t *ast        = ast_new_node();
    ast->type         = AST_TYPE_STRING;
    ast->ctype        = ast_new_array(ast_data_char, strlen(value) + 1);
    ast->string.data  = value;
    ast->string.label = ast_new_label();

    return ast;
}

////////////////////////////////////////////////////////////////////////
// variables (global and local)
ast_t *ast_new_variable_local(data_type_t *type, char *name) {
    ast_t *ast         = ast_new_node();
    ast->type          = AST_TYPE_VAR_LOCAL;
    ast->ctype         = type;
    ast->variable.name = name;

    if (ast_localenv)
        table_insert(ast_localenv, name, ast);
    if (ast_locals)
        list_push(ast_locals, ast);

    return ast;
}

ast_t *ast_new_variable_global(data_type_t *type, char *name) {
    ast_t *ast          = ast_new_node();
    ast->type           = AST_TYPE_VAR_GLOBAL;
    ast->ctype          = type;
    ast->variable.name  = name;
    ast->variable.label = name;

    table_insert(ast_globalenv, name, ast);
    return ast;
}

////////////////////////////////////////////////////////////////////////
// functions and calls
ast_t *ast_new_call(data_type_t *type, char *name, list_t *arguments, list_t *parametertypes) {
    ast_t *ast                   = ast_new_node();
    ast->type                    = AST_TYPE_CALL;
    ast->ctype                    = type;
    ast->function.call.paramtypes = parametertypes;
    ast->function.call.args       = arguments;
    ast->function.name            = name;

    return ast;
}

ast_t *ast_new_function(data_type_t *ret, char *name, list_t *params, ast_t *body, list_t *locals) {
    ast_t *ast           = ast_new_node();
    ast->type            = AST_TYPE_FUNCTION;
    ast->ctype           = ret;
    ast->function.name   = name;
    ast->function.params = params;
    ast->function.locals = locals;
    ast->function.body   = body;

    return ast;
}

////////////////////////////////////////////////////////////////////////
// declarations
ast_t *ast_new_decl(ast_t *var, ast_t *init) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_DECLARATION;
    ast->ctype     = NULL;
    ast->decl.var  = var;
    ast->decl.init = init;

    return ast;
}

////////////////////////////////////////////////////////////////////////
// constructs
ast_t *ast_new_initializerlist(list_t *init) {
    ast_t *ast         = ast_new_node();
    ast->type          = AST_TYPE_INITIALIZERLIST;
    ast->ctype         = NULL;
    ast->initlist.list = init;

    return ast;
}

data_type_t *ast_new_prototype(data_type_t *returntype, list_t *paramtypes, bool dots) {
    data_type_t *type  = (data_type_t*)malloc(sizeof(data_type_t));
    type->type         = TYPE_FUNCTION;
    type->returntype   = returntype;
    type->parameters   = paramtypes;
    type->hasdots      = dots;
    return type;
}

data_type_t *ast_new_array(data_type_t *type, int length) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_ARRAY;
    data->pointer     = type;
    data->size        = (length < 0) ? -1 : type->size * length;
    data->length      = length;

    return data;
}

data_type_t *ast_array_convert(data_type_t *type) {
    if (type->type != TYPE_ARRAY)
        return type;
    return ast_new_pointer(type->pointer);
}

data_type_t *ast_new_pointer(data_type_t *type) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_POINTER;
    data->pointer     = type;
    data->size        = 8;

    return data;
}

// while technically not a construct .. or an expression ternary
// can be considered a construct
ast_t *ast_new_ternary(data_type_t *type, ast_t *cond, ast_t *then, ast_t *last) {
    ast_t *ast       = ast_new_node();
    ast->type        = AST_TYPE_EXPRESSION_TERNARY;
    ast->ctype       = type;
    ast->ifstmt.cond = cond;
    ast->ifstmt.then = then;
    ast->ifstmt.last = last;

    return ast;
}

////////////////////////////////////////////////////////////////////////
// statements
static ast_t *ast_new_for_intermediate(int type, ast_t *init, ast_t *cond, ast_t *step, ast_t *body) {
    ast_t *ast        = ast_new_node();
    ast->type         = type;
    ast->ctype        = NULL;
    ast->forstmt.init = init;
    ast->forstmt.cond = cond;
    ast->forstmt.step = step;
    ast->forstmt.body = body;

    return ast;
}

ast_t *ast_new_jump(int type) {
    ast_t *ast  = ast_new_node();
    ast->type = type;
    return ast;
}

ast_t *ast_new_if(ast_t *cond, ast_t *then, ast_t *last) {
    ast_t *ast       = ast_new_node();
    ast->type        = AST_TYPE_STATEMENT_IF;
    ast->ctype       = NULL;
    ast->ifstmt.cond = cond;
    ast->ifstmt.then = then;
    ast->ifstmt.last = last;

    return ast;
}

ast_t *ast_new_for(ast_t *init, ast_t *cond, ast_t *step, ast_t *body) {
    return ast_new_for_intermediate(AST_TYPE_STATEMENT_FOR, init, cond, step, body);
}

ast_t *ast_new_while(ast_t *cond, ast_t *body) {
    return ast_new_for_intermediate(AST_TYPE_STATEMENT_WHILE, NULL, cond, NULL, body);
}

ast_t *ast_new_do(ast_t *cond, ast_t *body) {
    return ast_new_for_intermediate(AST_TYPE_STATEMENT_DO, NULL, cond, NULL, body);
}

ast_t *ast_new_return(data_type_t *returntype, ast_t *value) {
    ast_t *ast      = ast_new_node();
    ast->type       = AST_TYPE_STATEMENT_RETURN;
    ast->ctype      = returntype;
    ast->returnstmt = value;

    return ast;
}

ast_t *ast_new_compound(list_t *statements) {
    ast_t *ast    = ast_new_node();
    ast->type     = AST_TYPE_STATEMENT_COMPOUND;
    ast->ctype    = NULL;
    ast->compound = statements;

    return ast;
}

////////////////////////////////////////////////////////////////////////
// misc
char *ast_new_label(void) {
    string_t *string = string_create();
    string_catf(string, ".L%d", ast_label_index++);
    return string_buffer(string);
}

////////////////////////////////////////////////////////////////////////
// ast debugging facilities
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
        case TYPE_FUNCTION: return "<<todo>>";

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
    }
    return NULL;
}

static void ast_string_unary(string_t *string, const char *op, ast_t *ast) {
    string_catf(string, "(%s %s)", op, ast_string(ast->unary.operand));
}

static void ast_string_binary(string_t *string, const char *op, ast_t *ast) {
    string_catf(string, "(%s %s %s)", op, ast_string(ast->left), ast_string(ast->right));
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
            if (ast->decl.init)
                string_catf(string, " %s)", ast_string(ast->decl.init));
            else
                string_cat(string, ')');
            break;

        case AST_TYPE_STATEMENT_COMPOUND:
            string_cat(string, '{');
            for (list_iterator_t *it = list_iterator(ast->compound); !list_iterator_end(it); ) {
                ast_string_impl(string, list_iterator_next(it));
                string_cat(string, ';');
            }
            string_cat(string, '}');
            break;

        case AST_TYPE_INITIALIZERLIST:
            string_cat(string, '{');
            for(list_iterator_t *it = list_iterator(ast->initlist.list); !list_iterator_end(it); ) {
                ast_string_impl(string, list_iterator_next(it));
                if (!list_iterator_end(it))
                    string_cat(string, ',');
            }
            string_cat(string, '}');
            break;

        case AST_TYPE_STRUCT: // reference structure
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

        // statements
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
