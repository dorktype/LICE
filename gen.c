#include "gmcc.h"

// code generator


// registers for function call
static const char *registers[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};

void gen_emit_expression(FILE *as, ast_t *ast) {
    int i;
    switch (ast->type) {
        case ast_type_data_int:
            fprintf(as, "mov $%d, %%eax\n", ast->value.integer);
            break;

        case ast_type_data_var:
            fprintf(as, "mov -%d(%%rbp), %%eax\n", ast->value.variable->placement * 4);
            break;

        case ast_type_func_call:
            for (i = 1; i < ast->value.call.size; i++)
                fprintf(as, "push %%%s\n", registers[i]);
            for (i = 0; i < ast->value.call.size; i++) {
                gen_emit_expression(as, ast->value.call.args[i]);
                fprintf(as, "push %%rax\n");
            }
            for (i = ast->value.call.size - 1; i >= 0; i--)
                fprintf(as, "pop %%%s\n", registers[i]);
            fprintf(
                as,"\
                mov $0, %%eax\n\
                call %s\n",
                ast->value.call.name
            );
            for (i = ast->value.call.size - 1; i > 0; i--)
                fprintf(as, "pop %%%s\n", registers[i]);
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
