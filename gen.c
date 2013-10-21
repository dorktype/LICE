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

static int gen_type_shift(data_type_t *type) {
    switch (type->type) {
        case TYPE_CHAR: return 0;
        case TYPE_INT:  return 1;
        default:        return 3;
    }
    return 3;
}

static int gen_type_size(data_type_t *type) {
    return 1 << gen_type_shift(type);
}

void gen_emit_expression(FILE *as, ast_t *ast) {
    char *reg; // derefence register
    int   i;

    switch (ast->type) {
        case ast_type_data_literal:
            switch (ast->ctype->type) {
                case TYPE_INT:
                    fprintf(as, "mov $%d, %%eax\n", ast->integer);
                    break;
                case TYPE_CHAR:
                    fprintf(as, "mov $%d, %%rax\n", ast->character);
                    break;
                case TYPE_ARRAY:
                    fprintf(as, "mov .s%d(%%rip), %%rax\n", ast->string.id);
                    break;
                default:
                    compile_error("Internal error");
            }
            break;

        case ast_type_data_var:
            switch (gen_type_size(ast->ctype)) {
                case 1:
                    fprintf(as, "mov $0, %%eax\nmov -%d(%%rbp), %%al\n", ast->variable.placement * 8);
                    break;
                case 4:
                    fprintf(as, "mov -%d(%%rbp), %%eax\n", ast->variable.placement * 8);
                    break;
                case 8:
                    fprintf(as, "mov -%d(%%rbp), %%rax\n", ast->variable.placement * 8);
                    break;
                default:
                    compile_error("Internal error");
            }
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
                mov $0, %%eax\n\
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
            switch (gen_type_size(ast->ctype)) {
                case 1: reg = "%bl";  break;
                case 4: reg = "%ebx"; break;
                case 8: reg = "%rbx"; break;
                default:
                    compile_error("Internal error");
            }
            fprintf(
                as,"\
                mov $0, %%ebx\n\
                mov (%%rax), %s\n\
                mov %%rbx, %%rax\n",
                reg
            );
            break;

        default:
            gen_emit_binary(as, ast);
    }
}

static void gen_emit_pointer(FILE *as, char op, ast_t *left, ast_t *right) {
    int shift;

    gen_emit_expression(as, left);
    fprintf(as, "push %%rax\n");
    gen_emit_expression(as, right);

    if ((shift = gen_type_shift(left->ctype)) > 0)
        fprintf(as, "sal $%d, %%rax\n", shift);

    fprintf(
        as,"\
        mov %%rax, %%rbx\n\
        pop %%rax\n\
        add %%rbx, %%rax\n"
    );
}

void gen_emit_binary(FILE *as, ast_t *ast) {
    const char *operation;

    // emit binary store
    if (ast->type == '=') {
        gen_emit_assignment(as, ast->left, ast->right);
        return;
    }

    if (ast->ctype->type == TYPE_PTR) {
        gen_emit_pointer(as, ast->type, ast->left, ast->right);
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
