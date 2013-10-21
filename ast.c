#include <stdlib.h>

#include "gmcc.h"

// creates a new ast node
#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))

// list of strings
static ast_t *str_list = NULL;

ast_t *ast_strings(void) {
    return str_list;
}

ast_t *ast_new_bin_op(char type, ast_t *left, ast_t *right) {
    ast_t *ast = ast_new_node();
    ast->type  = type;
    ast->left  = left;
    ast->right = right;

    return ast;
}

ast_t *ast_new_func_call(char *name, int size, ast_t **nodes) {
    ast_t *ast           = ast_new_node();
    ast->type            = ast_type_func_call;
    ast->value.call.name = name;
    ast->value.call.size = size;
    ast->value.call.args = nodes;

    return ast;
}

ast_t *ast_new_data_var(var_t *var) {
    ast_t *ast          = ast_new_node();
    ast->type           = ast_type_data_var;
    ast->value.variable = var;

    return ast;
}

ast_t *ast_new_data_int(int value) {
    ast_t *ast         = ast_new_node();
    ast->type          = ast_type_data_int;
    ast->value.integer = value;

    return ast;
}

ast_t *ast_new_data_str(char *value) {
    ast_t *ast             = ast_new_node();
    ast->type              = ast_type_data_str;
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

void ast_dump_escape(const char *str) {
    while (*str) {
        if (*str == '\"' || *str == '\\')
            printf("\\");
        printf("%c", *str);
        str++;
    }
}

void ast_dump(ast_t *ast) {
    size_t i;
    switch (ast->type) {
        default:
            printf("(%c ", ast->type);
            ast_dump(ast->left); // dump the left node
            printf(" ");
            ast_dump(ast->right); // dump the right node
            printf(")"); // paren close the expression
            break;

        // data nodes
        case ast_type_data_int:
            printf("%d", ast->value.integer);
            break;
        case ast_type_data_var:
            printf("%s", ast->value.variable->name);
            break;

        case ast_type_data_str:
            printf("\"");
            ast_dump_escape(ast->value.string.data);
            printf("\"");
            break;

        case ast_type_func_call:
            printf("%s(", ast->value.call.name);
            for(i = 0; i < ast->value.call.size; i++) {
                ast_dump(ast->value.call.args[i]);
                if (ast->value.call.args[i + 1])
                    printf(", ");
            }
            printf(")");
            break;
    }
}
