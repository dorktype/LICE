#include <stdlib.h>

#include "gmcc.h"

static data_type_t *data_int  = &(data_type_t) { TYPE_INT,   NULL };
static data_type_t *data_char = &(data_type_t) { TYPE_CHAR,  NULL };
static data_type_t *data_str  = &(data_type_t)
{
    TYPE_ARRAY,
    &(data_type_t) {
        TYPE_CHAR,
        NULL
    }
};

data_type_t *ast_data_int(void)  { return data_int;  }
data_type_t *ast_data_char(void) { return data_char; }
data_type_t *ast_data_str(void)  { return data_str;  }

// creates a new ast node
#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))

// list of strings
ast_t *str_list = NULL;
ast_t *var_list = NULL;

// singletons
ast_t *ast_strings(void) {
    return str_list;
}
ast_t *ast_variables(void) {
    return var_list;
}

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

ast_t *ast_new_func_call(char *name, int size, ast_t **nodes) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_CALL;
    ast->ctype     = data_int;
    ast->call.name = name;
    ast->call.size = size;
    ast->call.args = nodes;

    return ast;
}

ast_t *ast_new_variable(data_type_t *type, char *name) {
    ast_t *ast              = ast_new_node();
    ast->type               = AST_TYPE_VAR;
    ast->ctype              = type;
    ast->variable.name      = name;
    ast->variable.placement = (var_list)
                                ? var_list->variable.placement + 1
                                : 1;
    ast->variable.next      = var_list;
    var_list                = ast;
    return ast;
}

ast_t *ast_new_int(int value) {
    ast_t *ast   = ast_new_node();
    ast->type    = AST_TYPE_LITERAL;
    ast->ctype   = data_int;
    ast->integer = value;

    return ast;
}

ast_t *ast_new_char(char value) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_LITERAL;
    ast->ctype     = data_char;
    ast->character = value;

    return ast;
}

ast_t *ast_new_string(char *value) {
    ast_t *ast       = ast_new_node();
    ast->type        = AST_TYPE_LITERAL;
    ast->ctype       = data_str;
    ast->string.data = value;

    if (!str_list) {
        ast->string.id   = 0;
        ast->string.next = NULL;
    } else {
        ast->string.id   = str_list->string.id + 1;
        ast->string.next = str_list;
    }

    str_list = ast;

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

data_type_t *ast_new_pointer(data_type_t *type) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_PTR;
    data->pointer     = type;

    return data;
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
            string_appendf(string, "%s[]", ast_type_string(type->pointer));
            return string_buffer(string);
    }
    return NULL;
}

static void ast_dump_string_impl(string_t *string, ast_t *ast) {
    char *left;
    char *right;

    size_t i;
    if (!ast) return;
    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_INT:
                    string_appendf(string, "%d", ast->integer);
                    break;
                case TYPE_CHAR:
                    string_appendf(string, "'%c'", ast->character);
                    break;
                case TYPE_ARRAY:
                    string_appendf(string, "\"%s\"", string_quote(ast->string.data));
                    break;
                default:
                    compile_error("Internal error %s", __func__);
                    break;
            }
            break;

        case AST_TYPE_VAR:
            string_appendf(string, "%s", ast->variable.name);
            break;

        case AST_TYPE_CALL:
            string_appendf(string, "%s(", ast->call.name);
            for(i = 0; i < ast->call.size; i++) {
                ast_dump_string_impl(string, ast->call.args[i]);
                if (ast->call.args[i + 1])
                    string_appendf(string, ",");
            }
            string_appendf(string, ")");
            break;

        case AST_TYPE_ADDR:
            string_appendf(string, "(& %s)", ast_dump_string(ast->unary.operand));
            break;

        case AST_TYPE_DEREF:
            string_appendf(string, "(* %s)", ast_dump_string(ast->unary.operand));
            break;

        case AST_TYPE_DECL:
            string_appendf(
                string,
                "(decl %s %s %s)",
                ast_type_string(ast->decl.var->ctype),
                ast->decl.var->variable.name,
                ast_dump_string(ast->decl.init)
            );
            break;

        default:
            left  = ast_dump_string(ast->left);
            right = ast_dump_string(ast->right);
            string_appendf(string, "(%c %s %s)", ast->type, left, right);
            break;
    }
}

char *ast_dump_string(ast_t *ast) {
    string_t *string = string_create();
    ast_dump_string_impl(string, ast);
    return string_buffer(string);
}
