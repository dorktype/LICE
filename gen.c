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

void gen_emit_impl(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int col = vprintf(fmt, args);
    va_end(args);

    for (const char *p = fmt; *p; p++)
        if (*p == '\t')
          col += 8 - 1;

    col = (40 - col) > 0 ? (40 - col) : 2;
    printf("%*c % 4d\n", col, '#', line);
}

static int gen_stack;
#define gen_push(X) gen_push_(X, __LINE__)
#define gen_pop(X) gen_pop_(X, __LINE__)
#define gen_push_xmm(X) gen_push_xmm_(X, __LINE__)
#define gen_pop_xmm(X) gen_pop_xmm_(X, __LINE__)

static void gen_push_(const char *reg, int line) {
    gen_emit_impl(line, "\tpush %%%s", reg);
    gen_stack += 8;
}
static void gen_pop_(const char *reg, int line) {
    gen_emit_impl(line, "\tpop %%%s", reg);
    gen_stack -= 8;
    if (gen_stack >= 0)
        printf("# stack misaligment reaches %d\n", gen_stack);
}
static void gen_push_xmm_(int r, int line) {
    gen_emit_impl(line, "\tsub $8, %%rsp");
    gen_emit_impl(line, "\tmovsd %%xmm%d, (%%rsp)", r);
    gen_stack += 8;
}
static void gen_pop_xmm_(int r, int line) {
    gen_emit_impl(line, "\tmovsd (%%rsp), %%xmm%d", r);
    gen_emit_impl(line, "\tadd $8, %%rsp");
    gen_stack -= 8;
    if (gen_stack >= 0)
        printf("# stack misalignment reaches %d\n", gen_stack);
}

static const char *gen_register_integer(data_type_t *type, char r) {
    switch (type->size) {
        case 1: return (r == 'a') ? "al"  : "cl";
        case 2: return (r == 'a') ? "ax"  : "cx";
        case 4: return (r == 'a') ? "eax" : "ecx";
        case 8: return (r == 'a') ? "rax" : "rcx";
    }
    return "<<lice internal error>>";
}

static void gen_expression(ast_t *ast);

static void gen_load_global(data_type_t *type, char *label, int offset) {
    if (type->type == TYPE_ARRAY) {
        if (offset)
            gen_emit("lea %s+%d(%%rip), %%rax", label, offset);
        else
            gen_emit("lea %s(%%rip), %%rax", label);
        return;
    }

    const char *reg = gen_register_integer(type, 'a');
    if (type->size < 4)
        gen_emit("mov $0, %%eax");
    if (offset)
        gen_emit("mov %s+%d(%%rip), %%%s", label, offset, reg);
    else
        gen_emit("mov %s(%%rip), %%%s", label, reg);
}

static void gen_cast_int(data_type_t *type) {
    if (!ast_type_floating(type))
        return;
    printf("# cast float/double to int {\n");
    gen_emit("cvttsd2si %%xmm0, %%eax");
    printf("# }\n");
}

static void gen_cast_float(data_type_t *type) {
    if (ast_type_floating(type))
        return;
    printf("# cast int to float/double {\n");
    gen_emit("cvtsi2sd %%eax, %%xmm0");
    printf("# }\n");
}

static void gen_load_local(data_type_t *var, int offset) {
    if (var->type == TYPE_ARRAY) {
        gen_emit("lea %d(%%rbp), %%rax", offset);
    } else if (var->type == TYPE_FLOAT) {
        gen_emit("cvtps2pd %d(%%rbp), %%xmm0", offset);
    } else if (var->type == TYPE_DOUBLE) {
        gen_emit("movsd %d(%%rbp), %%xmm0", offset);
    } else {
        const char *reg = gen_register_integer(var, 'a');
        if (var->size < 4)
            gen_emit("mov $0, %%eax");
        gen_emit("mov %d(%%rbp), %%%s", offset, reg);
    }
}

static void gen_save_global(char *name, data_type_t *type, int offset) {
    const char *reg = gen_register_integer(type, 'a');
    if (offset)
        gen_emit("mov %%%s, %s+%d(%%rip)", reg, name, offset);
    else
        gen_emit("mov %%%s, %s(%%rip)", reg, name);
}

static void gen_save_local(data_type_t *type, int offset) {
    if (type->name)
        printf("# saving local %s { \n", type->name);
    else
        printf("# saving local @%d of size %d\n", type->offset, type->size);

    if (type->type == TYPE_FLOAT) {
        gen_push_xmm(0);
        gen_emit("cvtpd2ps %%xmm0, %%xmm0");
        gen_emit("movss %%xmm0, %d(%%rbp)");
        gen_pop_xmm(0);
    }
    else if (type->type == TYPE_DOUBLE)
        gen_emit("movsd %%xmm0, %d(%%rbp)", offset);
    else
        gen_emit("mov %%%s, %d(%%rbp)", gen_register_integer(type, 'a'), offset);

    printf("#}\n");
}

// pointer dereferencing, load and assign
static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    printf("# pointer arithmetic {\n");
    gen_expression(left);
    gen_push("rax");
    gen_expression(right);

    int size = left->ctype->pointer->size;
    if (size > 1)
        gen_emit("imul $%d, %%rax", size);

    gen_emit("mov %%rax, %%rcx");
    gen_pop("rax");
    gen_emit("add %%rcx, %%rax");
    printf("#}\n");
}

static void gen_load_dereference(data_type_t *rtype, data_type_t *otype, int offset) {
    if (otype->type == TYPE_POINTER && otype->pointer->type == TYPE_ARRAY)
        return;

    const char *reg = gen_register_integer(rtype, 'c');
    if (rtype->size < 4)
        gen_emit("mov $0, %%ecx");
    if (offset)
        gen_emit("mov %d(%%rax), %%%s", offset, reg);
    else
        gen_emit("mov (%%rax), %%%s", reg);
    gen_emit("mov %%rcx, %%rax");
}

static void gen_assignment_dereference_intermediate(data_type_t *type, int offset) {
    gen_emit("mov (%%rsp), %%rcx");

    const char *reg = gen_register_integer(type, 'c');
    // clear 0!@?
    if (offset)
        gen_emit("mov %%%s, %d(%%rax)", reg, offset);
    else
        gen_emit("mov %%%s, (%%rax)", reg);
    gen_pop("rax");
}

static void gen_assignment_dereference(ast_t *var) {
    gen_push("rax");
    gen_expression(var->unary.operand);
    gen_assignment_dereference_intermediate(var->unary.operand->ctype->pointer, 0);
}

// a field inside the structure is just an offset assignment, but there
// could be pointers and what not so we need to deal with duplicate
// code here
static void gen_assignment_structure(ast_t *structure, data_type_t *field, int offset) {
    switch (structure->type) {
        case AST_TYPE_VAR_LOCAL:
            gen_save_local(field, structure->variable.off + field->offset + offset);
            break;

        case AST_TYPE_VAR_GLOBAL:
            gen_save_global(structure->variable.name, field, field->offset + offset);
            break;

        case AST_TYPE_STRUCT: // recursive
            gen_assignment_structure(structure->structure, field, offset + structure->field->offset);
            break;

        case AST_TYPE_DEREFERENCE:
            gen_push("rax");
            gen_expression(structure->unary.operand);
            gen_assignment_dereference_intermediate(field, field->offset + offset);
            break;

        default:
            compile_error("Internal error: gen_assignment_structure");
            break;
    }
}

static void gen_load_structure(ast_t *structure, data_type_t *field, int offset) {
    switch (structure->type) {
        case AST_TYPE_VAR_LOCAL:
            gen_load_local(field, structure->variable.off + field->offset + offset);
            break;

        case AST_TYPE_VAR_GLOBAL:
            gen_load_global(field, structure->variable.name, field->offset + offset);
            break;

        case AST_TYPE_STRUCT: // recursive
            gen_load_structure(structure->structure, field, offset + structure->field->offset + offset);
            break;

        case AST_TYPE_DEREFERENCE:
            gen_expression(structure->unary.operand);
            gen_load_dereference(structure->ctype, field, field->offset + offset);
            break;

        default:
            compile_error("Internal error: gen_assignment_structure");
            break;
    }
}

static void gen_assignment(ast_t *var) {
    switch (var->type) {
        case AST_TYPE_VAR_LOCAL:
            gen_save_local(var->ctype, var->variable.off);
            break;

        case AST_TYPE_VAR_GLOBAL:
            gen_save_global(var->variable.name, var->ctype, 0);
            break;

        case AST_TYPE_DEREFERENCE:
            gen_assignment_dereference(var);
            break;

        case AST_TYPE_STRUCT:
            gen_assignment_structure(var->structure, var->field, 0);
            break;

        default:
            compile_error("Internal error: gen_assignment");
    }
}

static void gen_comparision(char *operation, ast_t *ast) {
    if (ast_type_floating(ast->ctype)) {
        gen_expression(ast->left);
        gen_cast_float(ast->left->ctype);
        gen_push_xmm(0);
        gen_expression(ast->right);
        gen_cast_float(ast->right->ctype);
        gen_pop_xmm(1);
        gen_emit("ucomisd %%xmm0, %%xmm1");
    } else {
        gen_expression(ast->left);
        gen_cast_int(ast->left->ctype);
        gen_push("rax");
        gen_expression(ast->right);
        gen_cast_int(ast->right->ctype);
        gen_pop("rcx");
        gen_emit("cmp %%rax, %%rcx");
    }
    gen_emit("%s %%al", operation);
    gen_emit("movzb %%al, %%eax");
}

static void gen_binary_arithmetic_integer(ast_t *ast) {
    printf("# %s {\n", ast_string(ast));
    char *op;
    switch (ast->type) {
        case '+': op = "add";  break;
        case '-': op = "sub";  break;
        case '*': op = "imul"; break;
        case '/':
            break;
        default:
            compile_error("Internal error: gen_binary");
            break;
    }

    gen_expression(ast->left);
    gen_cast_int(ast->left->ctype);
    gen_push("rax");
    gen_expression(ast->right);
    gen_cast_int(ast->right->ctype);
    gen_emit("mov %%rax, %%rcx");
    gen_pop("rax");

    if (ast->type == '/') {
        gen_emit("mov $0, %%edx");
        gen_emit("idiv %%rcx");
    } else {
        gen_emit("%s %%rcx, %%rax", op);
    }

    printf("# }\n");
}

static void gen_binary_arithmetic_floating(ast_t *ast) {
    printf("# %s {\n", ast_string(ast));
    char *op;
    switch (ast->type) {
        case '+': op = "addsd"; break;
        case '-': op = "subsd"; break;
        case '*': op = "mulsd"; break;
        case '/': op = "divsd"; break;
        default:
            compile_error("Internal error: gen_binary");
            break;
    }

    gen_expression(ast->left);
    gen_cast_float(ast->left->ctype);
    gen_push_xmm(0);
    gen_expression(ast->right);
    gen_cast_float(ast->right->ctype);
    gen_emit("movsd %%xmm0, %%xmm1");
    gen_pop_xmm(0);
    gen_emit("%s %%xmm1, %%xmm0", op);
    printf("# }\n");
}

static void gen_binary(ast_t *ast) {
    if (ast->type == '=') {
        gen_expression(ast->right);
        if (ast_type_floating(ast->ctype))
            gen_cast_float(ast->right->ctype);
        else
            gen_cast_int(ast->right->ctype);
        gen_assignment(ast->left);
        return;
    }

    if (ast->type == LEXER_TOKEN_EQUAL) {
        printf("# %s {\n", ast_string(ast));
        gen_comparision("sete", ast);
        printf("# }\n");
        return;
    }

    if (ast->ctype->type == TYPE_POINTER) {
        gen_pointer_arithmetic(ast->type, ast->left, ast->right);
        return;
    }

    switch (ast->type) {
        case '<':                gen_comparision("setl",  ast); return;
        case '>':                gen_comparision("setg",  ast); return;
        case LEXER_TOKEN_LEQUAL: gen_comparision("setle", ast); return;
        case LEXER_TOKEN_GEQUAL: gen_comparision("setge", ast); return;
        case LEXER_TOKEN_NEQUAL: gen_comparision("setne", ast); return;
    }

    if (ast_type_integer(ast->ctype))
        gen_binary_arithmetic_integer(ast);
    else if (ast_type_floating(ast->ctype))
        gen_binary_arithmetic_floating(ast);
    else
        compile_error("Internal error");
}

static void gen_emit_postfix(ast_t *ast, const char *op) {
    gen_expression(ast->unary.operand);
    gen_push("rax");
    gen_emit("%s $1, %%rax", op);
    gen_assignment(ast->unary.operand);
    gen_pop("rax");
}

static int gen_alignment(int n, int align) {
    int remainder = n % align;
    return (remainder == 0)
                ? n
                : n - remainder + align;
}

// data generation
void gen_data_section(void) {
    gen_emit(".data");

    for (list_iterator_t *it = list_iterator(ast_globalenv->variables); !list_iterator_end(it); ) {
        ast_t *ast = list_iterator_next(it);
        if (ast->type == AST_TYPE_STRING) {
            gen_emit_label("%s:", ast->string.label);
            gen_emit_inline(".string \"%s\"", string_quote(ast->string.data));
        } else if (ast->type != AST_TYPE_VAR_GLOBAL) {
            compile_error("TODO: gen_data_section");
        }
    }

    // float and doubles
    for (list_iterator_t *it = list_iterator(ast_floats); !list_iterator_end(it); ) {
        ast_t *ast   = list_iterator_next(it);
        char  *label = ast_new_label();

        ast->floating.label = label;
        gen_emit_label("%s:", label);
        gen_emit(".long %d", ((int*)&ast->floating.value)[0]);
        gen_emit(".long %d", ((int*)&ast->floating.value)[1]);
    }
}

static void gen_data_integer(ast_t *data) {
    switch (data->ctype->size) {
        case 1: gen_emit(".byte %d", data->integer); break;
        case 2: gen_emit(".word %d", data->integer); break;
        case 4: gen_emit(".long %d", data->integer); break;
        case 8: gen_emit(".quad %d", data->integer); break;
        default:
            compile_error("Internal error: failed to generate data");
            break;
    }
}

static void gen_data(ast_t *ast) {
    gen_emit_label(".global %s", ast->decl.var->variable.name);
    gen_emit_label("%s:", ast->decl.var->variable.name);

    // emit the array initialization
    if (ast->decl.init->type == AST_TYPE_ARRAY_INIT) {
        for (list_iterator_t *it = list_iterator(ast->decl.init->array); !list_iterator_end(it); )
            gen_data_integer(list_iterator_next(it));
        return;
    }
    gen_data_integer(ast->decl.init);
}

static void gen_bss(ast_t *ast) {
    gen_emit(".lcomm %s, %d", ast->decl.var->variable.name, ast->decl.var->ctype->size);
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

    printf("# prologue {\n");
    gen_emit_inline(".text");
    gen_emit_inline(".global %s", ast->function.name);
    gen_emit_label("%s:", ast->function.name);
    gen_push("rbp"); // doesn't count towards misalignment
    gen_stack -= 8;
    gen_emit("mov %%rsp, %%rbp");

    int offset = 0;
    int regi   = 0;
    int regx   = 0;

    for (list_iterator_t *it = list_iterator(ast->function.params); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);

        if (value->ctype->type == TYPE_FLOAT) {
            gen_emit("cvtpd2ps %%xmm%d, %%xmm%d", regx, regx);
            gen_push_xmm(regx++);
        } else if (value->ctype->type == TYPE_DOUBLE) {
            gen_push_xmm(regx++);
        } else {
            gen_push(registers[regi++]);
        }
        offset -= gen_alignment(value->ctype->size, 8);
        value->variable.off = offset;
    }
    for (list_iterator_t *it = list_iterator(ast->function.locals); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);
        offset -= gen_alignment(value->ctype->size, 8);
        value->variable.off = offset;
    }

    if (offset)
        gen_emit("add $%d, %%rsp", offset);
    gen_stack += -(offset - 8); //enable later

    printf("# }\n");
}

static void gen_function_epilogue(void) {
    //gen_pop("rbp");
    printf("# epilogue {\n");
    gen_emit("leave");
    gen_emit("ret");
    printf("# }\n");
}

void gen_function(ast_t *ast) {
    gen_stack = 0;
    if (ast->type == AST_TYPE_FUNCTION) {
        gen_function_prologue(ast);
        gen_expression(ast->function.body);
        gen_function_epilogue();
    } else if (ast->type == AST_TYPE_DECLARATION) {
        gen_global(ast);
    } else {
        compile_error("TODO: gen_function");
    }
    //if (gen_stack > 0)
    //    compile_error("## cannot continue, stack is misaligned by %d (bytes)\n", gen_stack);
}

static void gen_expression(ast_t *ast);
static void gen_expression_logical(ast_t *ast) {
    char *end  = ast_new_label();
    bool  flop = !!(ast->type == LEXER_TOKEN_AND);
    char *j    = (flop) ? "je" : "jne";
    int   a1   = (flop) ? 0    : 1;

    gen_expression(ast->left);
    gen_emit("test %%rax, %%rax");
    gen_emit("mov $%d, %%rax", a1);
    gen_emit("%s %s", j, end);
    gen_expression(ast->right);
    gen_emit("test %%rax, %%rax");
    gen_emit("mov $%d, %%rax", a1);
    gen_emit("%s %s", j, end);
    gen_emit("mov $%d, %%rax", (a1) ? 0 : 1);
    gen_emit("%s:", end);
}

static void gen_expression(ast_t *ast) {
    char *begin;
    char *ne;
    char *end;

    int regi = 0, backi;
    int regx = 0, backx;
    int i;

    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_INT:
                    gen_emit("mov $%d, %%eax", ast->integer);
                    break;

                case TYPE_LONG:
                    gen_emit("mov $%lu, %%rax", (unsigned long)ast->integer);
                    break;

                case TYPE_CHAR:
                    gen_emit("mov $%d, %%rax", ast->character);
                    break;

                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                    gen_emit("movsd %s(%%rip), %%xmm0", ast->floating.label);
                    break;

                default:
                    compile_error("Internal error: gen_expression");
            }
            break;

        case AST_TYPE_STRING:
            gen_emit("lea %s(%%rip), %%rax", ast->string.label);
            break;

        case AST_TYPE_VAR_LOCAL:
            gen_load_local(ast->ctype, ast->variable.off);
            break;
        case AST_TYPE_VAR_GLOBAL:
            gen_load_global(ast->ctype, ast->variable.label, 0);
            break;

        case AST_TYPE_CALL:
            printf("# function call arguments {\n");
            for (list_iterator_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                ast_t *v = list_iterator_next(it);
                if (ast_type_floating(v->ctype))
                    gen_push_xmm(regx++);
                else
                    gen_push(registers[regi++]);
            }

            for (list_iterator_t *it = list_iterator(ast->function.call.args); !list_iterator_end(it); ) {
                ast_t *v = list_iterator_next(it);
                gen_expression(v);
                if (ast_type_floating(v->ctype))
                    gen_push_xmm(0);
                else
                    gen_push("rax");
            }

            // reverse
            backi = regi;
            backx = regx;

            for (list_iterator_t *it = list_iterator(list_reverse(ast->function.call.args)); !list_iterator_end(it); ) {
                ast_t *v = list_iterator_next(it);
                if (ast_type_floating(v->ctype)) {
                    gen_pop_xmm(--backx);
                } else {
                    gen_pop(registers[--backi]);
                }
            }
            printf("# }\n");

            printf("# function call {\n");
            gen_emit("mov $%d, %%eax", regx);

            if (gen_stack % 16)// TODO: fix
                gen_emit("sub $8, %%rsp");

            gen_emit("call %s", ast->function.name);

            if (gen_stack % 16)// TODO: fix
               gen_emit("add $8, %%rsp");

            printf("# }\n");


            printf("# function call restore {\n");
            for (list_iterator_t *it = list_iterator(list_reverse(ast->function.call.args)); !list_iterator_end(it); ) {
                ast_t *v = list_iterator_next(it);
                if (ast_type_floating(v->ctype)) {
                    gen_pop_xmm(--regx);
                } else {
                    gen_pop(registers[--regi]);
                }
            }
            printf("# }\n");

            break;

        case AST_TYPE_DECLARATION:
            if (!ast->decl.init)
                return;

            if (ast->decl.init->type == AST_TYPE_ARRAY_INIT) {
                i = 0;
                for (list_iterator_t *it = list_iterator(ast->decl.init->array); !list_iterator_end(it);) {
                    gen_expression(list_iterator_next(it));
                    gen_save_local(ast->decl.var->ctype->pointer, ast->decl.var->variable.off + i);
                    i += ast->decl.var->ctype->pointer->size;
                }
            } else if (ast->decl.var->ctype->type == TYPE_ARRAY) {
                char *p;
                for (i = 0, p = ast->decl.init->string.data; *p; p++, i++)
                    gen_emit("movb $%d, %d(%%rbp)", *p, ast->decl.var->variable.off + i);
                gen_emit("movb $0, %d(%%rbp)", ast->decl.var->variable.off + i);
            } else if (ast->decl.init->type == AST_TYPE_STRING) {
                gen_load_global(ast->decl.init->ctype, ast->decl.init->string.label, 0);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->variable.off);
            } else {
                gen_expression(ast->decl.init);
                gen_save_local(ast->decl.var->ctype, ast->decl.var->variable.off);
            }
            return;

        case AST_TYPE_ADDRESS:
            switch (ast->unary.operand->type) {
                case AST_TYPE_VAR_LOCAL:
                    gen_emit("lea %d(%%rbp), %%rax", ast->unary.operand->variable.off);
                    break;

                case AST_TYPE_VAR_GLOBAL:
                    gen_emit("lea %s(%%rip), %%rax", ast->unary.operand->variable.label);
                    break;

                default:
                    compile_error("Internal error");
                    break;
            }
            break;

        case AST_TYPE_DEREFERENCE:
            gen_expression(ast->unary.operand);
            gen_load_dereference(ast->ctype, ast->unary.operand->ctype, 0);
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
            if (ast->returnstmt)
                gen_expression(ast->returnstmt);
            gen_emit("leave");
            gen_emit("ret");
            break;

        case AST_TYPE_STATEMENT_COMPOUND:
            for (list_iterator_t *it = list_iterator(ast->compound); !list_iterator_end(it); )
                gen_expression(list_iterator_next(it));
            break;

        case AST_TYPE_STRUCT:
            gen_load_structure(ast->structure, ast->field, 0);
            break;

        case '!':
            gen_expression(ast->unary.operand);
            gen_emit("cmp $0, %%rax");
            gen_emit("sete %%al");
            gen_emit("movzb %%al, %%eax");
            break;

        case LEXER_TOKEN_AND:
        case LEXER_TOKEN_OR:
            gen_expression_logical(ast);
            break;

        case '&':
        case '|':
            gen_expression(ast->left);
            gen_push("rax");
            gen_expression(ast->right);
            gen_pop("rcx");
            gen_emit("%s %%rcx, %%rax", (ast->type == '|') ? "or" : "and");
            break;

        case LEXER_TOKEN_INCREMENT: gen_emit_postfix(ast, "add"); break;
        case LEXER_TOKEN_DECREMENT: gen_emit_postfix(ast, "sub"); break;

        default:
            gen_binary(ast);
    }
}
