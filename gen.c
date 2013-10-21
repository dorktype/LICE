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
    if (ast->type != ast_type_bin_add &&
        ast->type != ast_type_bin_sub &&
        ast->type != ast_type_data_int) {
        compile_error("Internal error: %s called without valid integer expression", __func__);
    }

    if (ast->type == ast_type_data_int)
        fprintf(as, "mov $%d, %%eax\n", ast->value.integer);
    else
        gen_emit_bin(as, ast);
}

void gen_emit_bin(FILE *as, ast_t *ast) {
    const char *operation;
    if (ast->type == ast_type_bin_add)
        operation = "add";
    else if (ast->type == ast_type_bin_sub)
        operation = "sub";
    else
        compile_error("Internal error: %s called with invalid binary operation", __func__);

    gen_emit_expression(as, ast->left);
    fprintf(as, "mov %%eax, %%ebx\n");
    gen_emit_expression(as, ast->right);
    fprintf(as, "%s %%ebx, %%eax\n", operation);
}
