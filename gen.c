#include "gmcc.h"

// code generator
void gen_emit_binary(FILE *as, ast_t *ast);

// registers for function call
static const char *registers[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};

void gen_emit_data(FILE *as, ast_t *strings) {
    ast_t *ast;

    if (!(ast = strings))
        return;

    fprintf(as, ".data\n");

    for (; ast; ast = ast->string.next) {
        fprintf(
            as,
            ".s%d:\n\
            .string \"%s\"\n",
            ast->string.id,
            string_quote(ast->string.data)
        );
    }
}

static void gen_emit_assignment(FILE *as, ast_t *left, ast_t *right) {
    gen_emit_expression(as, right);
    fprintf(
        as,"\
        mov %%rax, -%d(%%rbp)\n",
        left->variable.placement * 8
    );
}

void gen_emit_expression(FILE *as, ast_t *ast) {
    int i;

    switch (ast->type) {
        case ast_type_data_literal:
            switch (ast->ctype->type) {
                case TYPE_INT:
                    fprintf(as, "mov $%d, %%rax\n", ast->integer);
                    break;
                case TYPE_CHAR:
                    fprintf(as, "mov $%d, %%rax\n", ast->character);
                    break;
                case TYPE_STR:
                    fprintf(as, "mov .s%d(%%rip), %%rax\n", ast->string.id);
                    break;
                default:
                    compile_error("Internal error");
            }
            break;

        case ast_type_data_var:
            fprintf(as, "mov -%d(%%rbp), %%rax\n", ast->variable.placement * 8);
            break;

        case ast_type_func_call:
            for (i = 1; i < ast->call.size; i++)
                fprintf(as, "push %%%s\n", registers[i]);
            for (i = 0; i < ast->call.size; i++) {
                gen_emit_expression(as, ast->call.args[i]);
                fprintf(as, "push %%rax\n");
            }
            for (i = ast->call.size - 1; i >= 0; i--)
                fprintf(as, "pop %%%s\n", registers[i]);
            fprintf(
                as,"\
                mov $0, %%rax\n\
                call %s\n",
                ast->call.name
            );
            for (i = ast->call.size - 1; i > 0; i--)
                fprintf(as, "pop %%%s\n", registers[i]);
            break;

        case ast_type_decl:
            gen_emit_assignment(as, ast->decl.var, ast->decl.init);
            break;

        case ast_type_addr:
            fprintf(as, "lea -%d(%%rbp), %%rax\n", ast->unary.operand->variable.placement * 8);
            break;

        case ast_type_deref:
            gen_emit_expression(as, ast->unary.operand);
            fprintf(as, "mov (%%rax), %%rax\n");
            break;

        default:
            gen_emit_binary(as, ast);
    }
}

void gen_emit_binary(FILE *as, ast_t *ast) {
    const char *operation;

    // emit binary store
    if (ast->type == '=') {
        gen_emit_assignment(as, ast->left, ast->right);
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
            mov %%rax, %%rbx\n\
            pop %%rax\n\
            mov $0, %%edx\n\
            idiv %%rbx\n"
        );
    } else {
        fprintf(
            as,"\
            pop %%rbx\n\
            %s %%rbx, %%rax\n",
            operation
        );
    }
}
