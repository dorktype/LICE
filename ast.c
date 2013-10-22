#include <stdlib.h>
#include <string.h>

#include "gmcc.h"

static int ast_label_index = 0;

data_type_t *ast_data_int  = &(data_type_t) { TYPE_INT,  NULL };
data_type_t *ast_data_char = &(data_type_t) { TYPE_CHAR, NULL };
list_t      *ast_globals   = &(list_t){ .length = 0, .head = NULL, .tail = NULL };
list_t      *ast_locals    = NULL;
list_t      *ast_params    = NULL;

// creates a new ast node
#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))

ast_t *ast_new_unary(char type, data_type_t *data, ast_t *operand) {
    ast_t *ast         = ast_new_node();
    ast->type          = type;
    ast->ctype         = data;
    ast->unary.operand = operand;

    return ast;
}

ast_t *ast_new_binary(char type, data_type_t *data, ast_t *left, ast_t *right) {
    ast_t *ast         = ast_new_node();
    ast->type          = type;
    ast->ctype         = data;
    ast->right         = right;
    ast->left          = left;

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
    string_appendf(string, ".L%d", ast_label_index++);
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

ast_t *ast_new_reference_local(data_type_t *type, ast_t *var, int off) {
    ast_t *ast         = ast_new_node();
    ast->type          = AST_TYPE_REF_LOCAL;
    ast->ctype         = type;
    ast->local_ref.ref = var;
    ast->local_ref.off = off;

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

ast_t *ast_new_reference_global(data_type_t *type, ast_t *var, int off) {
    ast_t *ast          = ast_new_node();
    ast->type           = AST_TYPE_REF_GLOBAL;
    ast->ctype          = type;
    ast->global_ref.ref = var;
    ast->global_ref.off = off;

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

ast_t *ast_new_function(data_type_t *ret, char *name, list_t *params, list_t *body, list_t *locals) {
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
    ast->type      = AST_TYPE_DECL;
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

ast_t *ast_new_if(ast_t *cond, list_t *then, list_t *last) {
    ast_t *ast       = ast_new_node();
    ast->type        = AST_TYPE_IF;
    ast->ctype       = NULL;
    ast->ifstmt.cond = cond;
    ast->ifstmt.then = then;
    ast->ifstmt.last = last;

    return ast;
}

data_type_t *ast_new_pointer(data_type_t *type) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_PTR;
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

ast_t *ast_new_for(ast_t *init, ast_t *cond, ast_t *step, list_t *body) {
    ast_t *ast        = ast_new_node();
    ast->type         = AST_TYPE_FOR;
    ast->ctype        = NULL;
    ast->forstmt.init = init;
    ast->forstmt.cond = cond;
    ast->forstmt.step = step;
    ast->forstmt.body = body;

    return ast;
}

ast_t *ast_new_return(ast_t *value) {
    ast_t *ast      = ast_new_node();
    ast->type       = AST_TYPE_RETURN;
    ast->ctype      = NULL;
    ast->returnstmt = value;
}

static ast_t *ast_find_variable_subsitute(list_t *list, const char *name) {
    for (list_iter_t *it = list_iterator(list); !list_iterator_end(it); ) {
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

        case TYPE_PTR:
            string = string_create();
            string_appendf(string, "%s*", ast_type_string(type->pointer));
            return string_buffer(string);

        case TYPE_ARRAY:
            string = string_create();
            string_appendf(
                string,
                "%s[%d]",
                ast_type_string(type->pointer),
                type->size
            );
            return string_buffer(string);
    }
    return NULL;
}

static void ast_string_impl(string_t *string, ast_t *ast);
char *ast_block_string(list_t *block) {
    string_t *string = string_create();
    string_append(string, '{');
    for (list_iter_t *it = list_iterator(block); !list_iterator_end(it); ) {
        ast_string_impl(string, list_iterator_next(it));
        string_append(string, ';');
    }
    string_append(string, '}');
    return string_buffer(string);
}

static void ast_string_impl(string_t *string, ast_t *ast) {
    char *left;
    char *right;

    if (!ast) {
        string_appendf(string, "(null)");
        return;
    }

    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_INT:  string_appendf(string, "%d",   ast->integer);   break;
                case TYPE_CHAR: string_appendf(string, "'%c'", ast->character); break;
                default:
                    compile_error("Internal error");
                    break;
            }
            break;

        case AST_TYPE_STRING:
            string_appendf(string, "\"%s\"", string_quote(ast->string.data));
            break;

        case AST_TYPE_VAR_LOCAL:
            string_appendf(string, "%s", ast->local.name);
            break;

        case AST_TYPE_VAR_GLOBAL:
            string_appendf(string, "%s", ast->global.name);
            break;

        case AST_TYPE_REF_LOCAL:
            string_appendf(string, "%s[%d]", ast_string(ast->local_ref.ref), ast->local_ref.off);
            break;

        case AST_TYPE_REF_GLOBAL:
            string_appendf(string, "%s[%d]", ast_string(ast->global_ref.ref), ast->global_ref.off);
            break;

        case AST_TYPE_CALL:
            string_appendf(string, "(%s)%s(", ast_type_string(ast->ctype), ast->function.name);
            for (list_iter_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                string_appendf(string, "%s", ast_string(list_iterator_next(it)));
                if (!list_iterator_end(it))
                    string_append(string, ',');
            }
            string_append(string, ')');
            break;

        case AST_TYPE_FUNCTION:
            string_appendf(string, "(%s)%s(", ast_type_string(ast->ctype), ast->function.name);
            for (list_iter_t *it = list_iterator(ast->function.params); !list_iterator_end(it); ) {
                ast_t *param = list_iterator_next(it);
                string_appendf(string, "%s %s", ast_type_string(param->ctype), ast_string(param));
                if (!list_iterator_end(it))
                    string_append(string, ',');
            }
            string_appendf(string, ")%s", ast_block_string(ast->function.body));
            break;

        case AST_TYPE_DECL:
            string_appendf(string, "(decl %s %s %s)",
                    ast_type_string(ast->decl.var->ctype),
                    ast->decl.var->local.name,
                    ast_string(ast->decl.init)
            );
            break;

        case AST_TYPE_ARRAY_INIT:
            string_append(string, '{');
            for(list_iter_t *it = list_iterator(ast->array); !list_iterator_end(it); ) {
                ast_string_impl(string, list_iterator_next(it));
                if (!list_iterator_end(it))
                    string_append(string, ',');
            }
            string_append(string, '}');
            break;

        case AST_TYPE_ADDR:
            string_appendf(string, "(& %s)", ast_string(ast->unary.operand));
            break;

        case AST_TYPE_DEREF:
            string_appendf(string, "(* %s)", ast_string(ast->unary.operand));
            break;

        case AST_TYPE_IF:
            string_appendf(string, "(if %s %s", ast_string(ast->ifstmt.cond), ast_block_string(ast->ifstmt.then));
            if (ast->ifstmt.last)
                string_appendf(string, " %s", ast_block_string(ast->ifstmt.last));
            string_append(string, ')');
            break;

        case AST_TYPE_FOR:
            string_appendf(string, "(for %s %s %s %s)",
                ast_string(ast->forstmt.init),
                ast_string(ast->forstmt.cond),
                ast_string(ast->forstmt.step),
                ast_block_string(ast->forstmt.body)
            );
            break;

        case AST_TYPE_RETURN:
            string_appendf(string, "(return %s)", ast_string(ast->returnstmt));
            break;

        default:
            left  = ast_string(ast->left);
            right = ast_string(ast->right);
            string_appendf(string, "(%c %s %s)", ast->type, left, right);
            break;
    }
}

char *ast_string(ast_t *ast) {
    string_t *string = string_create();
    ast_string_impl(string, ast);
    return string_buffer(string);
}
