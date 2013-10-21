#include "gmcc.h"

// code generator
static void gen_string_quote(FILE *as, const char *p) {
    while (*p) {
        if (*p == '\"' || *p == '\\')
            fprintf(as, "\\");
        fprintf(as, "%c", *p);
        p++;
    }
}

void gen_emit_string(FILE *as, ast_t *ast) {
    fprintf(
        as,"\
        .data:\n\
        .gmcc_data:\n\
        .string\""
    );

    gen_string_quote(as, ast->value.string);

    fprintf(
        as,"\
        .text\n\
        .global %s\n\
        %s:\n\
            lea .gmcc_data(%%rip), %%rax\n\
            ret\n",
        GMCC_ENTRY_STR,
        GMCC_ENTRY_STR
    );
}

void gen_emit_expression(FILE *as, ast_t *ast) {
    if (ast->type == ast_type_data_int)
        fprintf(as, "mov $%d, %%eax\n", ast->value.integer);
    else
        gen_emit_bin(as, ast);
}

void gen_emit_bin(FILE *as, ast_t *ast) {
    const char *operation;
    switch (ast->type) {
        case '+': operation = "add";  break;
        case '-': operation = "sub";  break;

        case '*':
        case '/':
            operation = "imul";
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
