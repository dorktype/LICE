#include <stdlib.h>

#include "gmcc.h"

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

ast_t *ast_new_bin_op(char type, type_t ctype, ast_t *left, ast_t *right) {
    ast_t *ast = ast_new_node();
    ast->type  = type;
    ast->ctype = ctype;
    ast->left  = left;
    ast->right = right;

    return ast;
}

ast_t *ast_new_func_call(char *name, int size, ast_t **nodes) {
    ast_t *ast           = ast_new_node();
    ast->type            = ast_type_func_call;
    ast->ctype           = TYPE_INT;
    ast->value.call.name = name;
    ast->value.call.size = size;
    ast->value.call.args = nodes;

    return ast;
}

ast_t *ast_new_data_var(type_t type, char *name) {
    ast_t *ast                    = ast_new_node();
    ast->type                     = ast_type_data_var;
    ast->ctype                    = type;
    ast->value.variable.name      = name;
    ast->value.variable.placement = (var_list)
                                        ? var_list->value.variable.placement + 1
                                        : 1;
    ast->value.variable.next      = var_list;
    var_list                      = ast;
    return ast;
}

ast_t *ast_new_data_int(int value) {
    ast_t *ast         = ast_new_node();
    ast->type          = ast_type_data_int;
    ast->ctype         = TYPE_INT;
    ast->value.integer = value;

    return ast;
}

ast_t *ast_new_data_chr(char value) {
    ast_t *ast           = ast_new_node();
    ast->type            = ast_type_data_chr;
    ast->ctype           = TYPE_CHAR;
    ast->value.character = value;

    return ast;
}

ast_t *ast_new_data_str(char *value) {
    ast_t *ast             = ast_new_node();
    ast->type              = ast_type_data_str;
    ast->ctype             = TYPE_STR;
    ast->value.string.data = value;

    if (!str_list) {
        ast->value.string.id   = 0;
        ast->value.string.next = NULL;
    } else {
        ast->value.string.id   = str_list->value.string.id + 1;
        ast->value.string.next = str_list;
    }

    str_list = ast;

    return ast;
}

ast_t *ast_new_decl(ast_t *var, ast_t *init) {
    ast_t *ast     = ast_new_node();
    ast->type      = ast_type_decl;
    ast->decl.var  = var;
    ast->decl.init = init;

    return ast;
}

const char *ast_type_string(type_t type) {
    switch (type) {
        case TYPE_VOID: return "void";
        case TYPE_INT:  return "int";
        case TYPE_CHAR: return "char";
        case TYPE_STR:  return "string";
    }
    return NULL;
}

static void ast_dump_string_impl(string_t *string, ast_t *ast) {
    size_t i;
    if (!ast) return;
    switch (ast->type) {
        default:
            string_appendf(
                string,
                "(%c %s %s)",
                ast->type,
                ast_dump_string(ast->left),
                ast_dump_string(ast->right)
            );
            break;

        // data nodes
        case ast_type_data_int:
            string_appendf(string, "%d", ast->value.integer);
            break;
        case ast_type_data_var:
            string_appendf(string, "%s", ast->value.variable.name);
            break;

        case ast_type_data_str:
            string_appendf(string, "\"%s\"", string_quote(ast->value.string.data));
            break;

        case ast_type_data_chr:
            string_appendf(string, "'%c'", ast->value.character);
            break;

        case ast_type_func_call:
            string_appendf(string, "%s(", ast->value.call.name);
            for(i = 0; i < ast->value.call.size; i++) {
                ast_dump_string_impl(string, ast->value.call.args[i]);
                if (ast->value.call.args[i + 1])
                    string_appendf(string, ",");
            }
            string_appendf(string, ")");
            break;

        case ast_type_decl:
            string_appendf(
                string,
                "(decl %s %s %s)",
                ast_type_string(ast->decl.var->ctype),
                ast->decl.var->value.variable.name,
                ast_dump_string(ast->decl.init)
            );
            break;
    }
}

char *ast_dump_string(ast_t *ast) {
    string_t *string = string_create();
    ast_dump_string_impl(string, ast);
    return string_buffer(string);
}
