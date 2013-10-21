#include <stdlib.h>

#include "gmcc.h"

// creates a new ast node
#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))

ast_t *ast_new_bin_op(ast_type_t type, ast_t *left, ast_t *right) {
    ast_t *ast;

    ast        = ast_new_node();
    ast->type  = type;
    ast->left  = left;
    ast->right = right;

    return ast;
}

ast_t *ast_new_data_str(char *value) {
    ast_t *ast;

    ast               = ast_new_node();
    ast->type         = ast_type_data_str;
    ast->value.string = value;

    return ast;
}

ast_t *ast_new_data_int(int value) {
    ast_t *ast;

    ast                = ast_new_node();
    ast->type          = ast_type_data_int;
    ast->value.integer = value;

    return ast;
}

void ast_dump(ast_t *ast) {
    switch (ast->type) {
        case ast_type_bin_add:
            printf("(+ ");
            // fall
        case ast_type_bin_sub:
            if (ast->type == ast_type_bin_sub)
                printf("(- ");
            ast_dump(ast->left); // dump the left node
            printf(" ");
            ast_dump(ast->right); // dump the right node
            printf(")"); // paren close the expression
            break;

        // data nodes
        case ast_type_data_int:
            printf("%d", ast->value.integer);
            break;
        case ast_type_data_str:
            printf("%s", ast->value.string);
            break;
    }
}
