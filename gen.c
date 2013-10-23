#include <stdarg.h>
#include <string.h>
#include "lice.h"

// registers for function call
static const char *registers[] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};


#define gen_emit(...)        gen_emit_impl(__LINE__, "\t" __VA_ARGS__)
#define gen_emit_label(...)  gen_emit_impl(__LINE__,      __VA_ARGS__)
#define gen_emit_inline(...) gen_emit_impl(__LINE__,      __VA_ARGS__)

void gen_emit_impl(int line,  char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int col = vprintf(fmt, args);
    va_end(args);

    for (char *p = fmt; *p; p++)
        if (*p == '\t')
          col += 8 - 1;

    col = (30 - col) > 0 ? (30 - col) : 2;
    printf("%*c % 4d\n", col, '#', line);
}

static void gen_expression(ast_t *ast);
static void gen_pointer_dereference(ast_t *var);

static int gen_type_size(data_type_t *type) {
    switch (type->type) {
        case TYPE_CHAR: return 1;
        case TYPE_INT:  return 4;
        case TYPE_POINTER:  return 8;

        case TYPE_ARRAY:
            return gen_type_size(type->pointer) * type->size;

        default:
            compile_error("Internal error");
    }
    return 0;
}

static void gen_load_global(data_type_t *type, char *label) {
    if (type->type == TYPE_ARRAY) {
        gen_emit("lea %s(%%rip), %%rax", label);
        return;
    }

    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  gen_emit("mov $0, %%eax"); break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    gen_emit("mov %s(%%rip), %%%s", label, reg);
}

static void gen_load_local(ast_t *var) {
    if (var->ctype->type == TYPE_ARRAY) {
        gen_emit("lea %d(%%rbp), %%rax", -var->local.off);
        return;
    }

    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1:
            gen_emit("mov $0, %%eax");
            gen_emit("mov %d(%%rbp), %%al", -var->local.off);
            break;

        case 4:
            gen_emit("mov %d(%%rbp), %%eax", -var->local.off);
            break;

        case 8:
            gen_emit("mov %d(%%rbp), %%rax", -var->local.off);
            break;
    }
}

static void gen_save_global(ast_t *var) {
    char *reg;
    int size = gen_type_size(var->ctype);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }

    gen_emit("mov %%%s, %s(%%rip)", reg, var->global.name);
}

static void gen_save_local(data_type_t *type, int loff, int roff) {
    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }
    gen_emit("mov %%%s, %d(%%rbp)", reg, -(loff + roff * size));
}

static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    gen_expression(left);
    gen_emit("push %%rax");
    gen_expression(right);

    int size = gen_type_size(left->ctype->pointer);
    if (size > 1)
        gen_emit("imul $%d, %%rax", size);

    gen_emit("mov %%rax, %%rcx");
    gen_emit("pop %%rax");
    gen_emit("add %%rcx, %%rax");
}

static void gen_assignment(ast_t *var) {
    if (var->type == AST_TYPE_DEREFERENCE) {
        gen_pointer_dereference(var);
        return;
    }
    switch (var->type) {
        case AST_TYPE_VAR_LOCAL:
            gen_save_local(var->ctype, var->local.off, 0);
            break;

        case AST_TYPE_VAR_GLOBAL:
            gen_save_global(var);
            break;

        default:
            compile_error("Internal error");
    }
}

static void gen_comparision(char *operation, ast_t *a, ast_t *b) {
    gen_expression(a);
    gen_emit("push %%rax");
    gen_expression(b);

    gen_emit("pop %%rcx");
    gen_emit("cmp %%rax, %%rcx");
    gen_emit("%s %%al", operation);
    gen_emit("movzb %%al, %%eax");
}

static void gen_binary(ast_t *ast) {
    if (ast->type == '=') {
        gen_expression(ast->right);
        gen_assignment(ast->left);
        return;
    }

    if (ast->type == LEXER_TOKEN_EQUAL) {
        gen_comparision("sete", ast->left, ast->right);
        return;
    }

    if (ast->ctype->type == TYPE_POINTER) {
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
    gen_emit("push %%rax");
    gen_expression(ast->left);

    if (ast->type == '/') {
        gen_emit("pop %%rcx");
        gen_emit("mov $0, %%edx");
        gen_emit("idiv %%rcx");
    } else {
        gen_emit("pop %%rcx");
        gen_emit("%s %%rcx, %%rax", op);
    }
}

static void gen_emit_postfix(ast_t *ast, const char *op) {
    gen_expression(ast->unary.operand);
    gen_emit("push %%rax");
    gen_emit("%s $1, %%rax", op);
    gen_assignment(ast->unary.operand);
    gen_emit("pop %%rax");
}

static int gen_data_padding(int n) {
    int remainder = n % 8;
    return (remainder == 0)
                ? n
                : n - remainder + 8;
}

// data generation
void gen_data_section(void) {
    if (!ast_globals)
        return;

    for (list_iterator_t *it = list_iterator(ast_globals); !list_iterator_end(it); ) {
        ast_t *ast = list_iterator_next(it);
        if (ast->type == AST_TYPE_STRING) {
            gen_emit_label("%s:", ast->string.label);
            gen_emit_inline(".string \"%s\"", string_quote(ast->string.data));
        } else if (ast->type != AST_TYPE_VAR_GLOBAL) {
            compile_error("TODO");
        }
    }
}

static void gen_data_integer(ast_t *data) {
    switch (gen_type_size(data->ctype)) {
        case 1: gen_emit(".byte %d", data->integer); break;
        case 4: gen_emit(".long %d", data->integer); break;
        case 8: gen_emit(".quad %d", data->integer); break;
        default:
            compile_error("Internal error: failed to generate data");
            break;
    }
}

static void gen_data(ast_t *ast) {
    gen_emit_label(".global %s", ast->decl.var->global.name);
    gen_emit_label("%s:", ast->decl.var->global.name);

    // emit the array initialization
    if (ast->decl.init->type == AST_TYPE_ARRAY_INIT) {
        for (list_iterator_t *it = list_iterator(ast->decl.init->array); !list_iterator_end(it); )
            gen_data_integer(list_iterator_next(it));
        return;
    }
    gen_data_integer(ast->decl.init);
}

static void gen_bss(ast_t *ast) {
    gen_emit(".lcomm %s, %d", ast->decl.var->global.name, gen_type_size(ast->decl.var->ctype));
}

static void gen_global(ast_t *var) {
    if (var->decl.init) {
        gen_data(var);
    } else {
        gen_bss(var);
    }
}

static void gen_function_prologue(ast_t *ast) {
    if (list_length(ast->function.params) > sizeof(registers)/sizeof(registers[0]))
        compile_error("Too many params for function");

    gen_emit_inline(".text");
    gen_emit_inline(".global %s", ast->function.name);
    gen_emit_label("%s:", ast->function.name);
    gen_emit("push %%rbp");
    gen_emit("mov %%rsp, %%rbp");

    int r = 0;
    int o = 0;

    for (list_iterator_t *it = list_iterator(ast->function.params); !list_iterator_end(it); r++) {
        ast_t *value = list_iterator_next(it);
        gen_emit("push %%%s", registers[r]);
        o += gen_data_padding(gen_type_size(value->ctype));
        value->local.off = o;
    }

    for (list_iterator_t *it = list_iterator(ast->function.locals); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);
        o += gen_data_padding(gen_type_size(value->ctype));
        value->local.off = o;
    }

    if (o)
        gen_emit("sub $%d, %%rsp", o);
}

static void gen_function_epilogue(void) {
    gen_emit("leave");
    gen_emit("ret");
}

void gen_function(ast_t *ast) {
    if (ast->type == AST_TYPE_FUNCTION) {
        gen_function_prologue(ast);
        gen_expression(ast->function.body);
        gen_function_epilogue();
    } else if (ast->type == AST_TYPE_DECLARATION) {
        gen_global(ast);
    } else {
        compile_error("TODO");
    }
}

static void gen_pointer_dereference(ast_t *var) {
    char *reg;

    gen_emit("push %%rax");
    gen_expression(var->unary.operand);
    gen_emit("pop %%rcx");

    switch (gen_type_size(var->unary.operand->ctype)) {
        case 1: reg = "cl";  break;
        case 4: reg = "ecx"; break;
        case 8: reg = "rcx"; break;
    }

    gen_emit("mov %%%s, (%%rax)", reg);
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
                    gen_emit("mov $%d, %%eax", ast->integer);
                    break;
                case TYPE_CHAR:
                    gen_emit("mov $%d, %%rax", ast->character);
                    break;

                default:
                    compile_error("Internal error");
            }
            break;

        case AST_TYPE_STRING:
            gen_emit("lea %s(%%rip), %%rax", ast->string.label);
            break;

        case AST_TYPE_VAR_LOCAL:
            gen_load_local(ast);
            break;
        case AST_TYPE_VAR_GLOBAL:
            gen_load_global(ast->ctype, ast->global.label);
            break;

        case AST_TYPE_CALL:
            for (i = 1; i < list_length(ast->function.call.args); i++)
                gen_emit("push %%%s", registers[i]);
            for (list_iterator_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                gen_expression(list_iterator_next(it));
                gen_emit("push %%rax");
            }
            for (i = list_length(ast->function.call.args) - 1; i >= 0; i--)
                gen_emit("pop %%%s", registers[i]);
            gen_emit("mov $0, %%eax");
            gen_emit("call %s", ast->function.name);
            for (i = list_length(ast->function.call.args) - 1; i > 0; i--)
                gen_emit("pop %%%s", registers[i]);
            break;

        case AST_TYPE_DECLARATION:
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
                    gen_emit("movb $%d, %d(%%rbp)", *p, -(ast->decl.var->local.off - i));
                gen_emit("movb $0, %d(%%rbp)", -(ast->decl.var->local.off - i));
            } else if (ast->decl.init->type == AST_TYPE_STRING) {
                gen_load_global(ast->decl.init->ctype, ast->decl.init->string.label);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            } else {
                gen_expression(ast->decl.init);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->local.off, 0);
            }
            return;

        case AST_TYPE_ADDRESS:
            gen_emit("lea %d(%%rbp), %%rax", -ast->unary.operand->local.off);
            break;

        case AST_TYPE_DEREFERENCE:
            gen_expression(ast->unary.operand);
            switch (gen_type_size(ast->ctype)) {
                case 1:
                    // special
                    gen_emit("mov $0, %%ecx");
                    r = "cl";
                    break;

                case 4:
                    r = "ecx";
                    break;

                // larger than 8 uses largest possible register
                // e.g arrays
                case 8:
                default:
                    r = "rcx";
                    break;
            }
            // don't emit for arrays
            if (ast->unary.operand->ctype->pointer->type != TYPE_ARRAY) {
                gen_emit("mov (%%rax), %%%s", r);
                gen_emit("mov %%rcx, %%rax");
            }
            break;

        case AST_TYPE_STATEMENT_IF:
        case AST_TYPE_EXPRESSION_TERNARY:
            gen_expression(ast->ifstmt.cond);
            ne = ast_new_label();
            gen_emit("test %%rax, %%rax");
            gen_emit("je %s", ne);
            gen_expression(ast->ifstmt.then);
            if (ast->ifstmt.last) {
                end = ast_new_label();
                gen_emit("jmp %s", end);
                gen_emit_label("%s:", ne);
                gen_expression(ast->ifstmt.last);
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
                gen_emit("test %%rax, %%rax");
                gen_emit("je %s", end);
            }
            gen_expression(ast->forstmt.body);
            if (ast->forstmt.step)
                gen_expression(ast->forstmt.step);
            gen_emit("jmp %s", begin);
            gen_emit_label("%s:", end);
            break;

        case AST_TYPE_STATEMENT_RETURN:
            gen_expression(ast->returnstmt);
            gen_emit("leave");
            gen_emit("ret");
            break;

        case AST_TYPE_STATEMENT_COMPOUND:
            for (list_iterator_t *it = list_iterator(ast->compound); !list_iterator_end(it); )
                gen_expression(list_iterator_next(it));
            break;

        case '!':
            gen_expression(ast->unary.operand);
            gen_emit("cmp $0, %%rax");
            gen_emit("sete %%al");
            gen_emit("movzb %%al, %%eax");
            break;

        case LEXER_TOKEN_INCREMENT: gen_emit_postfix(ast, "add"); break;
        case LEXER_TOKEN_DECREMENT: gen_emit_postfix(ast, "sub"); break;

        default:
            gen_binary(ast);
    }
}
