#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "lice.h"

static int ast_label_index = 0;

data_type_t *ast_data_int  = &(data_type_t) { TYPE_INT,  NULL };
data_type_t *ast_data_char = &(data_type_t) { TYPE_CHAR, NULL };
list_t      *ast_globals   = &(list_t){ .length = 0, .head = NULL, .tail = NULL };
list_t      *ast_locals    = NULL;
list_t      *ast_params    = NULL;


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
        if (a->type != TYPE_INT)
            goto error;

        return b;
    }

    switch (a->type) {
        case TYPE_VOID:
            goto error;
        case TYPE_INT:
        case TYPE_CHAR:
            switch (b->type) {
                case TYPE_INT:
                case TYPE_CHAR:
                    return ast_data_int;
                case TYPE_ARRAY:
                case TYPE_POINTER:
                    return b;
                case TYPE_VOID:
                    goto error;
            }
            compile_error("Internal error");
            break;
        case TYPE_ARRAY:
            if (b->type != TYPE_ARRAY)
                goto error;
            return ast_result_type_impl(jmpbuf, op, a->pointer, b->pointer);
        default:
            compile_error("Internal error");
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

// creates a new ast node
#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))

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

ast_t *ast_new_int(int value) {
    ast_t *ast   = ast_new_node();
    ast->type    = AST_TYPE_LITERAL;
    ast->ctype   = ast_data_int;
    ast->integer = value;

    return ast;
}

ast_t *ast_new_char(char value) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_LITERAL;
    ast->ctype     = ast_data_char;
    ast->character = value;

    return ast;
}

char *ast_new_label(void) {
    string_t *string = string_create();
    string_catf(string, ".L%d", ast_label_index++);
    return string_buffer(string);
}

ast_t *ast_new_variable_local(data_type_t *type, char *name) {
    ast_t *ast      = ast_new_node();
    ast->type       = AST_TYPE_VAR_LOCAL;
    ast->ctype      = type;
    ast->local.name = name;

    if (ast_locals)
        list_push(ast_locals, ast);
    return ast;
}

ast_t *ast_new_variable_global(data_type_t *type, char *name, bool file) {
    ast_t *ast        = ast_new_node();
    ast->type         = AST_TYPE_VAR_GLOBAL;
    ast->ctype        = type;
    ast->global.name  = name;
    ast->global.label = (file) ? ast_new_label() : name;

    list_push(ast_globals, ast);
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

ast_t *ast_new_call(data_type_t *type, char *name, list_t *args) {
    ast_t *ast              = ast_new_node();
    ast->type               = AST_TYPE_CALL;
    ast->ctype              = type;
    ast->function.call.args = args;
    ast->function.name      = name;

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

ast_t *ast_new_decl(ast_t *var, ast_t *init) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_DECLARATION;
    ast->ctype     = NULL;
    ast->decl.var  = var;
    ast->decl.init = init;

    return ast;
}

ast_t *ast_new_array_init(list_t *init) {
    ast_t *ast  = ast_new_node();
    ast->type   = AST_TYPE_ARRAY_INIT;
    ast->ctype  = NULL;
    ast->array  = init;

    return ast;
}

data_type_t *ast_array_convert(data_type_t *type) {
    if (type->type != TYPE_ARRAY)
        return type;
    return ast_new_pointer(type->pointer);
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

data_type_t *ast_new_pointer(data_type_t *type) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_POINTER;
    data->pointer     = type;

    return data;
}

data_type_t *ast_new_array(data_type_t *type, int size) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_ARRAY;
    data->pointer     = type;
    data->size        = size;

    return data;
}

ast_t *ast_new_for(ast_t *init, ast_t *cond, ast_t *step, ast_t *body) {
    ast_t *ast        = ast_new_node();
    ast->type         = AST_TYPE_STATEMENT_FOR;
    ast->ctype        = NULL;
    ast->forstmt.init = init;
    ast->forstmt.cond = cond;
    ast->forstmt.step = step;
    ast->forstmt.body = body;

    return ast;
}

ast_t *ast_new_return(ast_t *value) {
    ast_t *ast      = ast_new_node();
    ast->type       = AST_TYPE_STATEMENT_RETURN;
    ast->ctype      = NULL;
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

ast_t *ast_new_ternary(data_type_t *type, ast_t *cond, ast_t *then, ast_t *last) {
    ast_t *ast       = ast_new_node();
    ast->type        = AST_TYPE_EXPRESSION_TERNARY;
    ast->ctype       = type;
    ast->ifstmt.cond = cond;
    ast->ifstmt.then = then;
    ast->ifstmt.last = last;

    return ast;
}

static ast_t *ast_find_variable_subsitute(list_t *list, const char *name) {
    for (list_iterator_t *it = list_iterator(list); !list_iterator_end(it); ) {
        ast_t *v = list_iterator_next(it);
        if (!strcmp(name, v->local.name))
            return v;
    }
    return NULL;
}

ast_t *ast_find_variable(const char *name) {
    ast_t *r;

    if ((r = ast_find_variable_subsitute(ast_locals,  name))) return r;
    if ((r = ast_find_variable_subsitute(ast_params,  name))) return r;
    if ((r = ast_find_variable_subsitute(ast_globals, name))) return r;

    return NULL;
}

const char *ast_type_string(data_type_t *type) {
    string_t *string;

    switch (type->type) {
        case TYPE_VOID: return "void";
        case TYPE_INT:  return "int";
        case TYPE_CHAR: return "char";

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
                type->size
            );
            return string_buffer(string);
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////
// ast dump

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
                case TYPE_INT:  string_catf(string, "%d",   ast->integer);   break;
                case TYPE_CHAR: string_catf(string, "'%c'", ast->character); break;
                default:
                    compile_error("Internal error");
                    break;
            }
            break;

        case AST_TYPE_STRING:
            string_catf(string, "\"%s\"", string_quote(ast->string.data));
            break;

        case AST_TYPE_VAR_LOCAL:
            string_catf(string, "%s", ast->local.name);
            break;

        case AST_TYPE_VAR_GLOBAL:
            string_catf(string, "%s", ast->global.name);
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
                    ast->decl.var->local.name
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

        case AST_TYPE_ARRAY_INIT:
            string_cat(string, '{');
            for(list_iterator_t *it = list_iterator(ast->array); !list_iterator_end(it); ) {
                ast_string_impl(string, list_iterator_next(it));
                if (!list_iterator_end(it))
                    string_cat(string, ',');
            }
            string_cat(string, '}');
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
