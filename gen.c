#include "gmcc.h"

// code generator

void gen_emit_expression(FILE *as, ast_t *ast) {
    switch (ast->type) {
        case ast_type_data_int:
            fprintf(as, "mov $%d, %%eax\n", ast->value.integer);
            break;

        case ast_type_data_var:
            fprintf(as, "mov -%d(%%rbp), %%eax\n", ast->value.variable->placement * 4);
            break;

        default:
            gen_emit_bin(as, ast);
    }
}

void gen_emit_bin(FILE *as, ast_t *ast) {
    const char *operation;

    // emit binary store
    if (ast->type == '=') {
        gen_emit_expression(as, ast->right);
        if (ast->left->type != ast_type_data_var)
            compile_error("Expected variable");
        // sizeof(int) == 4, hence * 4
        fprintf(as, "mov %%eax, -%d(%%rbp)\n", ast->left->value.variable->placement * 4);
        return;
    }

    switch (ast->type) {
        case '+': operation = "add"; break;
        case '-': operation = "sub"; break;

        case '*':
            operation = "imul";
            break;

        // handle specially
        case '/':
            break;
    }

    gen_emit_expression(as, ast->left);
    fprintf(as, "push %%rax\n");
    gen_emit_expression(as, ast->right);

    // deal with div specially
    if (ast->type == '/') {
        fprintf(
            as,"\
            mov %%eax, %%ebx\n\
            pop %%rax\n\
            mov $0, %%edx\n\
            idiv %%ebx\n"
        );
    } else {
        fprintf(
            as,"\
            pop %%rbx\n\
            %s %%ebx, %%eax\n",
            operation
        );
    }
}
