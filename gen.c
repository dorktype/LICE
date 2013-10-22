#include <stdarg.h>
#include <string.h>
#include "lice.h"

// registers for function call
static const char *registers[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};


#define gen_emit(A,...)      gen_emit_impl(__LINE__, A, "\t" __VA_ARGS__)
#define gen_emit_basic(...)  gen_emit_impl(__LINE__, 0, "\t" __VA_ARGS__)
#define gen_emit_label(...)  gen_emit_impl(__LINE__, 0,  __VA_ARGS__)
#define gen_emit_inline(...) gen_emit_impl(__LINE__, 0,  __VA_ARGS__)

void gen_emit_impl(int line, const char *annotate, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int col = vprintf(fmt, args);
    va_end(args);

    for (char *p = fmt; *p; p++)
        if (*p == '\t')
          col += 8 - 1;

    col = (30 - col) > 0 ? (30 - col) : 2;
    if (annotate)
        printf("%*c % 4d %s\n", col, '#', line, annotate);
    else
        printf("%*c % 4d\n", col, '#', line);
}

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
        gen_emit("array global load", "lea %s(%%rip), %%rax", label);
        if (off)
            gen_emit("array global load offset", "add $%d, %%rax", gen_type_size(type->pointer) * off);
        return;
    }

    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  gen_emit_basic("mov $0, %%eax"); break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    gen_emit("global load", "mov %s(%%rip), %%%s", label, reg);
    if (off)
        gen_emit("global load offset", "add $%d, %%rax", off * size);
    gen_emit("global load end", "mov (%%rax), %%%s", reg);
}

static void gen_load_local(ast_t *var, int off) {
    if (var->ctype->type == TYPE_ARRAY) {
        gen_emit("local load array", "lea %d(%%rbp), %%rax", -var->local.off);
        return;
    }

    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1:
            gen_emit("local load", "mov $0, %%eax");
            gen_emit("local load", "mov %d(%%rbp), %%al", -var->local.off);
            break;

        case 4:
            gen_emit("local load", "mov %d(%%rbp), %%eax", -var->local.off);
            break;

        case 8:
            gen_emit("local load", "mov %d(%%rbp), %%rax", -var->local.off);
            break;
    }

    if (off)
        gen_emit("local load offset", "add $%d, %%rax", var->local.off * size);
}

static void gen_save_global(ast_t *var, int off) {
    char *reg;
    gen_emit("global save", "push %%rcx");
    gen_emit_basic("mov %s(%%rip), %%rcx", var->global.label);
    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    gen_emit("global save", "mov %s, %d(%%rbp)", reg, off * size);
    gen_emit("global save", "pop %%rcx");
}

static void gen_save_local(data_type_t *type, int loff, int roff) {
    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }
    gen_emit("local save", "mov %%%s, %d(%%rbp)", reg, -(loff + roff * size));
}

static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    gen_expression(left);
    gen_emit("pointer arithmetic", "push %%rax");
    gen_expression(right);

    int size = gen_type_size(left->ctype->pointer);
    if (size > 1)
        gen_emit("pointer arithmetic", "imul $%d, %%rax", size);

    gen_emit("pointer arithmetic", "mov %%rax, %%rcx");
    gen_emit("pointer arithmetic", "pop %%rax");
    gen_emit("pointer arithmetic", "add %%rcx, %%rax");
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

static void gen_comparision(char *operation, ast_t *a, ast_t *b) {
    gen_expression(a);
    gen_emit("comparision", "push %%rax");
    gen_expression(b);

    gen_emit("comparision", "pop %%rcx");
    gen_emit("comparision", "cmp %%rax, %%rcx");
    gen_emit("comparision", "%s %%al", operation);
    gen_emit("comparision", "movzb %%al, %%eax");
}

static void gen_binary(ast_t *ast) {
    if (ast->type == '=') {
        gen_assignment(ast->left, ast->right);
        return;
    }

    if (ast->type == ':') {
        gen_comparision("sete", ast->left, ast->right);
        return;
    }

    if (ast->ctype->type == TYPE_PTR) {
        gen_pointer_arithmetic(ast->type, ast->left, ast->right);
        return;
    }

    char *op;
    switch (ast->type) {
        case '<':
            gen_comparision("setl", ast->left, ast->right);
            return;
        case '>':
            gen_comparision("setg", ast->left, ast->right);
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

    gen_expression(ast->right);
    gen_emit("operation", "push %%rax");
    gen_expression(ast->left);

    if (ast->type == '/') {
        gen_emit("operation", "pop %%rcx");
        gen_emit("operation", "mov $0, %%edx");
        gen_emit("operation", "idiv %%rcx");
    } else {
        gen_emit("operation", "pop %%rcx");
        gen_emit("operation", "%s %%rcx, %%rax", op);
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

    for (list_iterator_t *it = list_iterator(ast_globals); !list_iterator_end(it); ) {
        ast_t *ast = list_iterator_next(it);
        gen_emit_label("%s:", ast->string.label);
        gen_emit_inline(".string \"%s\"", string_quote(ast->string.data));
    }
}

static void gen_function_prologue(ast_t *ast) {
    if (list_length(ast->function.params) > sizeof(registers)/sizeof(registers[0]))
        compile_error("Too many params for function");

    gen_emit_inline(".text");
    gen_emit_inline(".global %s", ast->function.name);
    gen_emit_label("%s:", ast->function.name);
    gen_emit("function prologue", "push %%rbp");
    gen_emit("function prologue", "mov %%rsp, %%rbp");

    int r = 0;
    int o = 0;

    for (list_iterator_t *it = list_iterator(ast->function.params); !list_iterator_end(it); r++) {
        ast_t *value = list_iterator_next(it);
        gen_emit("function prologue", "push %%%s", registers[r]);
        o += gen_data_padding(gen_type_size(value->ctype));
        value->local.off = o;
    }

    for (list_iterator_t *it = list_iterator(ast->function.locals); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);
        o += gen_data_padding(gen_type_size(value->ctype));
        value->local.off = o;
    }

    if (o)
        gen_emit("function prologue", "sub $%d, %%rsp", o);
}

static void gen_function_epilogue(void) {
    gen_emit("function epilogue", "leave");
    gen_emit("function epilogue", "ret");
}

void gen_function(ast_t *ast) {
    gen_function_prologue(ast);
    gen_block(ast->function.body);
    gen_function_epilogue();
}

static void gen_block(list_t *block) {
    for (list_iterator_t *it = list_iterator(block); !list_iterator_end(it); )
        gen_expression(list_iterator_next(it));
}

static void gen_pointer_dereference(ast_t *var, ast_t *value) {
    char *reg;

    gen_expression(var->unary.operand);
    gen_emit_basic("push %%rax");
    gen_expression(value);
    gen_emit_basic("pop %%rcx");

    switch (gen_type_size(var->unary.operand->ctype)) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    gen_emit("pointer dereference", "mov %%%s, (%%rcx)", reg);
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
                    gen_emit("literal integer expression", "mov $%d, %%eax", ast->integer);
                    break;
                case TYPE_CHAR:
                    gen_emit("literal integer expression", "mov $%d, %%rax", ast->character);
                    break;

                default:
                    compile_error("Internal error");
            }
            break;

        case AST_TYPE_STRING:
            gen_emit("string", "lea %s(%%rip), %%rax", ast->string.label);
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
                gen_emit("global reference", "lea %s(%%rip), %%rax", ast->global_ref.ref->string.label);
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
                gen_emit("function call", "push %%%s", registers[i]);
            for (list_iterator_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                gen_expression(list_iterator_next(it));
                gen_emit("function call", "push %%rax");
            }
            for (i = list_length(ast->function.call.args) - 1; i >= 0; i--)
                gen_emit("function call", "pop %%%s", registers[i]);
            gen_emit("function call", "mov $0, %%eax");
            gen_emit("function call", "call %s", ast->function.name);
            for (i = list_length(ast->function.call.args) - 1; i > 0; i--)
                gen_emit("function call", "pop %%%s", registers[i]);
            break;

        case AST_TYPE_DECL:
            if (!ast->decl.init)
                return;

            if (ast->decl.init->type == AST_TYPE_ARRAY_INIT) {
                i = 0;
                for (list_iterator_t *it = list_iterator(ast->decl.init->array); !list_iterator_end(it);) {
                    gen_expression(list_iterator_next(it));
                    gen_save_local(ast->decl.var->ctype->pointer, ast->decl.var->local.off, -i);
                    i++;
                }
            } else if (ast->decl.var->ctype->type == TYPE_ARRAY) {
                char *p;
                for (i = 0, p = ast->decl.init->string.data; *p; p++, i++)
                    gen_emit("array decl", "movb $%d, %d(%%rbp)", *p, -(ast->decl.var->local.off - i));
                gen_emit("array decl", "movb $0, %d(%%rbp)", -(ast->decl.var->local.off - i));
            } else if (ast->decl.init->type == AST_TYPE_STRING) {
                gen_load_global(ast->decl.init->ctype, ast->decl.init->string.label, 0);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            } else {
                gen_expression(ast->decl.init);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            }
            return;

        case AST_TYPE_ADDR:
            gen_emit_basic("address of", "lea %d(%%rbp), %%rax", -ast->unary.operand->local.off);
            break;

        case AST_TYPE_DEREF:
            gen_expression(ast->unary.operand);
            switch (gen_type_size(ast->ctype)) {
                case 1: r = "%cl";  break;
                case 4: r = "%ecx"; break;
                case 8: r = "%rcx"; break;
            }
            gen_emit("dereference", "mov $0, %%ecx");
            gen_emit("dereference", "mov (%%rax), %s", r);
            gen_emit("dereference", "mov %%rcx, %%rax");
            break;

        case AST_TYPE_STATEMENT_IF:
            gen_expression(ast->ifstmt.cond);
            ne = ast_new_label();
            gen_emit("if statement", "test %%rax, %%rax");
            gen_emit("if statement", "je %s", ne);
            gen_block(ast->ifstmt.then);
            if (ast->ifstmt.last) {
                end = ast_new_label();
                gen_emit("if statement", "jmp %s", end);
                gen_emit_label("%s:", ne);
                gen_block(ast->ifstmt.last);
                gen_emit_label("%s:", end);
            } else {
                gen_emit_label("%s:", ne);
            }
            break;

        case AST_TYPE_STATEMENT_FOR:
            if (ast->forstmt.init)
                gen_expression(ast->forstmt.init);
            begin = ast_new_label();
            end   = ast_new_label();
            gen_emit_label("%s:", begin);
            if (ast->forstmt.cond) {
                gen_expression(ast->forstmt.cond);
                gen_emit("for loop condition", "test %%rax, %%rax");
                gen_emit("for loop condition", "je %s", end);
            }
            gen_block(ast->forstmt.body);
            if (ast->forstmt.step)
                gen_expression(ast->forstmt.step);
            gen_emit("for loop", "jmp %s", begin);
            gen_emit_label("%s:", end);
            break;

        case AST_TYPE_STATEMENT_RETURN:
            gen_expression(ast->returnstmt);
            gen_emit_basic("leave");
            gen_emit_basic("ret");
            break;

        default:
            gen_binary(ast);
    }
}
