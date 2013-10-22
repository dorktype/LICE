#include "gmcc.h"

// registers for function call
static const char *registers[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};

static int gen_type_size(data_type_t *type) {
    switch (type->type) {
        case TYPE_CHAR: return 1;
        case TYPE_INT:  return 4;
        case TYPE_PTR:  return 8;

        case TYPE_ARRAY:
            return gen_type_size(type->pointer) * type->size;

        default:
            compile_error("Internal error");
    }
    return 0;
}

static void gen_load_global(data_type_t *type, char *label, int off) {
    if (type->type == TYPE_ARRAY) {
        printf("lea %s(%%rip), %%rax\n\t", label);
        if (off)
            printf("add $%d, %%rax\n\t", gen_type_size(type->pointer) * off);
        return;
    }

    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al"; printf("mov $0, %%eax\n\t"); break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    printf("mov %s(%%rip), %%%s\n\t", label, reg);
    if (off)
        printf("add $%d, %%rax\n\t", off * size);
    printf("mov (%%rax), %%%s\n\t", reg);
}

static void gen_load_local(ast_t *var, int off) {
    if (var->ctype->type == TYPE_ARRAY) {
        printf("lea -%d(%%rbp), %%rax\n\t", var->local.off);
        return;
    }

    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1:
            printf("mov $0, %%eax\n\t");
            printf("mov -%d(%%rbp), %%al\n\t", var->local.off);
            break;

        case 4:
            printf("mov -%d(%%rbp), %%eax\n\t", var->local.off);
            break;

        case 8:
            printf("mov -%d(%%rbp), %%rax\n\t", var->local.off);
            break;
    }

    if (off)
        printf("add $%d, %%rax\n\t", var->local.off * size);
}

static void gen_save_global(ast_t *var, int off) {
    char *reg;
    printf("push %%rbx\n\t");
    printf("mov %s(%%rip), %%rbx\n\t", var->global.label);
    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    printf("mov %s, %d(%%rbp)\n\t", reg, off * size);
    printf("pop %%rbx\n\t");
}

static void gen_save_local(data_type_t *type, int loff, int roff) {
    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }
    printf("mov %%%s, -%d(%%rbp)\n\t", reg, loff + roff * size);
}

static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    gen_expression(left);
    printf("push %%rax\n\t");
    gen_expression(right);

    int size = gen_type_size(left->ctype->pointer);
    if (size > 1)
        printf("imul $%d, %%rax\n\t", size);

    printf("mov %%rax, %%rbx\n\t");
    printf("pop %%rax\n\t");
    printf("add %%rbx, %%rax\n\t");
}

static void gen_assignment(ast_t *var, ast_t *value) {
    gen_expression(value);
    switch (var->type) {
        case AST_TYPE_VAR_LOCAL:
            gen_save_local(var->ctype, var->local.off, 0);
            break;

        case AST_TYPE_REF_LOCAL:
            gen_save_local(
                var->local_ref.ref->ctype,
                var->local_ref.ref->local.off,
                var->local_ref.off
            );
            break;

        case AST_TYPE_VAR_GLOBAL:
            gen_save_global(var, 0);
            break;

        case AST_TYPE_REF_GLOBAL:
            gen_save_global(var->global_ref.ref, var->global_ref.off);
            break;

        default:
            compile_error("Internal error");
    }
}

static void gen_binary(ast_t *ast) {
    if (ast->type == '=') {
        gen_assignment(ast->left, ast->right);
        return;
    }

    if (ast->ctype->type == TYPE_PTR) {
        gen_pointer_arithmetic(ast->type, ast->left, ast->right);
        return;
    }

    char *op;
    switch (ast->type) {
        case '+': op = "add";  break;
        case '-': op = "sub";  break;
        case '*': op = "imul"; break;

        case '/':
            break;

        default:
            compile_error("Internal error");
            break;
    }

    gen_expression(ast->left);
    printf("push %%rax\n\t");
    gen_expression(ast->right);

    if (ast->type == '/') {
        printf("mov %%rax, %%rbx\n\t");
        printf("pop %%rax\n\t");
        printf("mov $0, %%edx\n\t");
        printf("idiv %%rbx\n\t");
    } else {
        printf("pop %%rbx\n\t");
        printf("%s %%rbx, %%rax\n\t", op);
    }
}

void gen_data_section(void) {
    ast_t *ast;

    if (!ast_data_globals())
        return;

    for (ast = ast_data_globals(); ast; ast = ast->next) {
        printf("%s:\n\t", ast->string.label);
        printf(".string \"%s\"\n", string_quote(ast->string.data));
    }
    printf("\t");
}

void gen_expression(ast_t *ast) {
    char *r;
    int   i;

    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_INT:
                    printf("mov $%d, %%eax\n\t", ast->integer);
                    break;
                case TYPE_CHAR:
                    printf("mov $%d, %%rax\n\t", ast->character);
                    break;

                default:
                    compile_error("Internal error");
            }
            break;

        case AST_TYPE_STRING:
            printf("lea %s(%%rip), %%rax\n\t", ast->string.label);
            break;

        case AST_TYPE_VAR_LOCAL:
            gen_load_local(ast, 0);
            break;
        case AST_TYPE_REF_LOCAL:
            gen_load_local(ast->local_ref.ref, ast->local_ref.off);
            break;
        case AST_TYPE_VAR_GLOBAL:
            gen_load_global(ast->ctype, ast->global.label, 0);
            break;
        case AST_TYPE_REF_GLOBAL:
            if (ast->global_ref.ref->type == AST_TYPE_STRING)
                printf("lea %s(%%rip), %%Rax\n\t", ast->global_ref.ref->string.label);
            else {
                gen_load_global(
                    ast->global_ref.ref->ctype,
                    ast->global_ref.ref->global.label,
                    ast->global_ref.off
                );
            }
            break;

        case AST_TYPE_CALL:
            for (i = 1; i < ast->call.size; i++)
                printf("push %%%s\n\t", registers[i]);
            for (i = 0; i < ast->call.size; i++) {
                gen_expression(ast->call.args[i]);
                printf("push %%rax\n\t");
            }
            for (i = ast->call.size - 1; i >= 0; i--)
                printf("pop %%%s\n\t", registers[i]);
            printf("mov $0, %%eax\n\t");
            printf("call %s\n\t", ast->call.name);
            for (i = ast->call.size - 1; i > 0; i--)
                printf("pop %%%s\n\t", registers[i]);
            break;

        case AST_TYPE_DECL:
            if (ast->decl.init->type == AST_TYPE_ARRAY_INIT) {
                for (i = 0; i < ast->decl.init->array.size; i++) {
                    gen_expression(ast->decl.init->array.init[i]);
                    gen_save_local(ast->decl.var->ctype->pointer, ast->decl.var->local.off, -i);
                }
            } else if (ast->decl.var->ctype->type == TYPE_ARRAY) {
                char *p;
                for (i = 0, p = ast->decl.init->string.data; *p; p++, i++)
                    printf("movb $%d, -%d(%%rbp)\n\t", *p, ast->decl.var->local.off - i);
                printf("movb $0, -%d(%%rbp)\n\t", ast->decl.var->local.off - i);
            } else if (ast->decl.init->type == AST_TYPE_STRING) {
                gen_load_global(ast->decl.init->ctype, ast->decl.init->string.label, 0);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            } else {
                gen_expression(ast->decl.init);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            }
            return;

        case AST_TYPE_ADDR:
            printf("lea -%d(%%rbp), %%rax\n\t", ast->unary.operand->local.off);
            break;

        case AST_TYPE_DEREF:
            gen_expression(ast->unary.operand);
            switch (gen_type_size(ast->ctype)) {
                case 1: r = "%bl";  break;
                case 4: r = "%ebx"; break;
                case 8: r = "%rbx"; break;
            }
            printf("mov $0, %%ebx\n\t");
            printf("mov (%%rax), %s\n\t", r);
            printf("mov %%rbx, %%rax\n\t");
            break;

        default:
            gen_binary(ast);
    }
}
