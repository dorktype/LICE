#include "gmcc.h"

// registers for function call
static const char *registers[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};

static void gen_expression(ast_t *ast);
static void gen_pointer_dereference(ast_t *var, ast_t *value);
static void gen_block(list_t *block);

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
        printf("lea %d(%%rbp), %%rax\n\t", -var->local.off);
        return;
    }

    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1:
            printf("mov $0, %%eax\n\t");
            printf("mov %d(%%rbp), %%al\n\t", -var->local.off);
            break;

        case 4:
            printf("mov %d(%%rbp), %%eax\n\t", -var->local.off);
            break;

        case 8:
            printf("mov %d(%%rbp), %%rax\n\t", -var->local.off);
            break;
    }

    if (off)
        printf("add $%d, %%rax\n\t", var->local.off * size);
}

static void gen_save_global(ast_t *var, int off) {
    char *reg;
    printf("push %%rcx\n\t");
    printf("mov %s(%%rip), %%rcx\n\t", var->global.label);
    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    printf("mov %s, %d(%%rbp)\n\t", reg, off * size);
    printf("pop %%rcx\n\t");
}

static void gen_save_local(data_type_t *type, int loff, int roff) {
    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }
    printf("mov %%%s, %d(%%rbp)\n\t", reg, -(loff + roff * size));
}

static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    gen_expression(left);
    printf("push %%rax\n\t");
    gen_expression(right);

    int size = gen_type_size(left->ctype->pointer);
    if (size > 1)
        printf("imul $%d, %%rax\n\t", size);

    printf("mov %%rax, %%rcx\n\t");
    printf("pop %%rax\n\t");
    printf("add %%rcx, %%rax\n\t");
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

        case AST_TYPE_DEREF:
            gen_pointer_dereference(var, value);
            break;

        default:
            compile_error("Internal error");
    }
}

static void gen_comparision(ast_t *a, ast_t *b) {
    gen_expression(a);
    printf("push %%rax\n\t");
    gen_expression(b);

    printf("pop %%rcx\n\t");
    printf("cmp %%rax, %%rcx\n\t");
    printf("setl %%al\n\t");
    printf("movzb %%al, %%eax\n\t");
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
        case '<':
            gen_comparision(ast->left, ast->right);
            return;
        case '>':
            gen_comparision(ast->right, ast->left);
            return;

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
        printf("mov %%rax, %%rcx\n\t");
        printf("pop %%rax\n\t");
        printf("mov $0, %%edx\n\t");
        printf("idiv %%rcx\n\t");
    } else {
        printf("pop %%rcx\n\t");
        printf("%s %%rcx, %%rax\n\t", op);
    }
}

static int gen_data_padding(int n) {
    int remainder = n % 8;
    return (remainder == 0)
                ? n
                : n - remainder + 8;
}


void gen_data_section(void) {
    if (!ast_globals)
        return;

    for (list_iter_t *it = list_iterator(ast_globals); !list_iterator_end(it); ) {
        ast_t *ast = list_iterator_next(it);
        printf("%s:\n\t", ast->string.label);
        printf(".string \"%s\"\n", string_quote(ast->string.data));
    }
}

static void gen_function_prologue(ast_t *ast) {
    if (list_length(ast->function.params) > sizeof(registers)/sizeof(registers[0]))
        compile_error("Too many params for function");

    printf(".text\n\t");
    printf(".global %s\n", ast->function.name);
    printf("%s:\n\t", ast->function.name);
    printf("push %%rbp\n\t");
    printf("mov %%rsp, %%rbp\n\t");

    int r = 0;
    int o = 0;

    for (list_iter_t *it = list_iterator(ast->function.params); !list_iterator_end(it); r++) {
        ast_t *value = list_iterator_next(it);
        printf("push %%%s\n\t", registers[r]);
        o += gen_data_padding(gen_type_size(value->ctype));
        value->local.off = o;
    }

    for (list_iter_t *it = list_iterator(ast->function.locals); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);
        o += gen_data_padding(gen_type_size(value->ctype));
        value->local.off = o;
    }

    if (o)
        printf("sub $%d, %%rsp\n\t", o);
}

static void gen_function_epilogue(void) {
    printf("leave\n\t");
    printf("ret\n");
}

void gen_function(ast_t *ast) {
    gen_function_prologue(ast);
    gen_block(ast->function.body);
    gen_function_epilogue();
}

static void gen_block(list_t *block) {
    for (list_iter_t *it = list_iterator(block); !list_iterator_end(it); )
        gen_expression(list_iterator_next(it));
}

static void gen_pointer_dereference(ast_t *var, ast_t *value) {
    char *reg;

    gen_expression(var->unary.operand);
    printf("push %%rax\n\t");
    gen_expression(value);
    printf("pop %%rcx\n\t");

    switch (gen_type_size(var->unary.operand->ctype)) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    printf("mov %%%s, (%%rcx)\n\t", reg);
}

static void gen_expression(ast_t *ast) {
    char *begin;
    char *ne;
    char *end;

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
                printf("lea %s(%%rip), %%rax\n\t", ast->global_ref.ref->string.label);
            else {
                gen_load_global(
                    ast->global_ref.ref->ctype,
                    ast->global_ref.ref->global.label,
                    ast->global_ref.off
                );
            }
            break;

        case AST_TYPE_CALL:
            for (i = 1; i < list_length(ast->function.call.args); i++)
                printf("push %%%s\n\t", registers[i]);
            for (list_iter_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                gen_expression(list_iterator_next(it));
                printf("push %%rax\n\t");
            }
            for (i = list_length(ast->function.call.args) - 1; i >= 0; i--)
                printf("pop %%%s\n\t", registers[i]);
            printf("mov $0, %%eax\n\t");
            printf("call %s\n\t", ast->function.name);
            for (i = list_length(ast->function.call.args) - 1; i > 0; i--)
                printf("pop %%%s\n\t", registers[i]);
            break;

        case AST_TYPE_DECL:
            if (ast->decl.init->type == AST_TYPE_ARRAY_INIT) {
                i = 0;
                for (list_iter_t *it = list_iterator(ast->decl.init->array); !list_iterator_end(it);) {
                    gen_expression(list_iterator_next(it));
                    gen_save_local(ast->decl.var->ctype->pointer, ast->decl.var->local.off, -i);
                    i++;
                }
            } else if (ast->decl.var->ctype->type == TYPE_ARRAY) {
                char *p;
                for (i = 0, p = ast->decl.init->string.data; *p; p++, i++)
                    printf("movb $%d, %d(%%rbp)\n\t", *p, -(ast->decl.var->local.off - i));
                printf("movb $0, %d(%%rbp)\n\t", -(ast->decl.var->local.off - i));
            } else if (ast->decl.init->type == AST_TYPE_STRING) {
                gen_load_global(ast->decl.init->ctype, ast->decl.init->string.label, 0);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            } else {
                gen_expression(ast->decl.init);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            }
            return;

        case AST_TYPE_ADDR:
            printf("lea %d(%%rbp), %%rax\n\t", -ast->unary.operand->local.off);
            break;

        case AST_TYPE_DEREF:
            gen_expression(ast->unary.operand);
            switch (gen_type_size(ast->ctype)) {
                case 1: r = "%cl";  break;
                case 4: r = "%ecx"; break;
                case 8: r = "%rcx"; break;
            }
            printf("mov $0, %%ecx\n\t");
            printf("mov (%%rax), %s\n\t", r);
            printf("mov %%rcx, %%rax\n\t");
            break;

        case AST_TYPE_IF:
            gen_expression(ast->ifstmt.cond);
            ne = ast_new_label();
            printf("test %%rax, %%rax\n\t");
            printf("je %s\n\t", ne);
            gen_block(ast->ifstmt.then);
            if (ast->ifstmt.last) {
                end = ast_new_label();
                printf("jmp %s\n\t", end);
                printf("%s:\n\t", ne);
                gen_block(ast->ifstmt.last);
                printf("%s:\n\t", end);
            } else {
                printf("%s:\n\t", ne);
            }
            break;

        case AST_TYPE_FOR:
            if (ast->forstmt.init)
                gen_expression(ast->forstmt.init);
            begin = ast_new_label();
            end   = ast_new_label();
            printf("%s:\n\t", begin);
            if (ast->forstmt.cond) {
                gen_expression(ast->forstmt.cond);
                printf("test %%rax, %%rax\n\t");
                printf("je %s\n\t", end);
            }
            gen_block(ast->forstmt.body);
            if (ast->forstmt.step)
                gen_expression(ast->forstmt.step);
            printf("jmp %s\n\t", begin);
            printf("%s:\n\t", end);
            break;

        case AST_TYPE_RETURN:
            gen_expression(ast->returnstmt);
            printf("leave\n\t");
            printf("ret\n");
            break;

        default:
            gen_binary(ast);
    }
}
