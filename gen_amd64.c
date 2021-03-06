#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lice.h"

static const char *registers[] = {
    "rdi", "rsi", "rdx",
    "rcx", "r8",  "r9"
};

static void gen_expression(ast_t *);
static void gen_declaration_initialization(list_t *, int);

#define gen_emit(...)        gen_emit_impl(__LINE__, "\t" __VA_ARGS__)
#define gen_emit_inline(...) gen_emit_impl(__LINE__,      __VA_ARGS__)
#define gen_push(X)          gen_push_    (X, __LINE__)
#define gen_pop(X)           gen_pop_     (X, __LINE__)
#define gen_push_xmm(X)      gen_push_xmm_(X, __LINE__)
#define gen_pop_xmm(X)       gen_pop_xmm_ (X, __LINE__)

static int   gen_stack = 0;

static char *gen_label_break          = NULL;
static char *gen_label_continue       = NULL;
static char *gen_label_break_store    = NULL;
static char *gen_label_continue_store = NULL;
static char *gen_label_switch         = NULL;
static char *gen_label_switch_store   = NULL;

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

static void gen_jump_save(char *lbreak, char *lcontinue) {
    gen_label_break_store    = gen_label_break;
    gen_label_continue_store = gen_label_continue;
    gen_label_break          = lbreak;
    gen_label_continue       = lcontinue;
}

static void gen_jump_restore(void) {
    gen_label_break    = gen_label_break_store;
    gen_label_continue = gen_label_continue_store;
}

static void gen_push_(const char *reg, int line) {
    gen_emit_impl(line, "\tpush %%%s", reg);
    gen_stack += 8;
}
static void gen_pop_(const char *reg, int line) {
    gen_emit_impl(line, "\tpop %%%s", reg);
    gen_stack -= 8;
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
    gen_emit("cvttsd2si %%xmm0, %%eax");
}

static void gen_cast_float(data_type_t *type) {
    if (ast_type_floating(type))
        return;
    gen_emit("cvtsi2sd %%eax, %%xmm0");
}

static void gen_load_local(data_type_t *var, const char *base, int offset) {
    if (var->type == TYPE_ARRAY) {
        gen_emit("lea %d(%%%s), %%rax", offset, base);
    } else if (var->type == TYPE_FLOAT) {
        gen_emit("cvtps2pd %d(%%%s), %%xmm0", offset, base);
    } else if (var->type == TYPE_DOUBLE || var->type == TYPE_LDOUBLE) {
        gen_emit("movsd %d(%%%s), %%xmm0", offset, base);
    } else {
        const char *reg = gen_register_integer(var, 'c');
        if (var->size < 4)
            gen_emit("mov $0, %%ecx");
        gen_emit("mov %d(%%%s), %%%s", offset, base, reg);
        gen_emit("mov %%rcx, %%rax");
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
    if (type->type == TYPE_FLOAT) {
        gen_push_xmm(0);
        gen_emit("unpcklpd %%xmm0, %%xmm0");
        gen_emit("cvtpd2ps %%xmm0, %%xmm0");
        gen_emit("movss %%xmm0, %d(%%rbp)", offset);
        gen_pop_xmm(0);
    }
    else if (type->type == TYPE_DOUBLE || type->type == TYPE_LDOUBLE)
        gen_emit("movsd %%xmm0, %d(%%rbp)", offset);
    else
        gen_emit("mov %%%s, %d(%%rbp)", gen_register_integer(type, 'a'), offset);
}

static void gen_assignment_dereference_intermediate(data_type_t *type, int offset) {
    gen_emit("mov (%%rsp), %%rcx");

    const char *reg = gen_register_integer(type, 'c');
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

static void gen_ensure_lva(ast_t *ast) {
    if (ast->variable.init)
        gen_declaration_initialization(ast->variable.init, ast->variable.off);
    ast->variable.init = NULL;
}

static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    gen_expression(left);
    gen_push("rax");
    gen_expression(right);

    int size = left->ctype->pointer->size;
    if (size > 1)
        gen_emit("imul $%d, %%rax", size);

    gen_emit("mov %%rax, %%rcx");
    gen_pop("rax");
    gen_emit("add %%rcx, %%rax");
}

static void gen_assignment_structure(ast_t *structure, data_type_t *field, int offset) {
    switch (structure->type) {
        case AST_TYPE_VAR_LOCAL:
            gen_ensure_lva(structure);
            gen_save_local(field, structure->variable.off + field->offset + offset);
            break;

        case AST_TYPE_VAR_GLOBAL:
            gen_save_global(structure->variable.name, field, field->offset + offset);
            break;

        case AST_TYPE_STRUCT:
            gen_assignment_structure(structure->structure, field, offset + structure->ctype->offset);
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
            gen_ensure_lva(structure);
            gen_load_local(field, "rbp", structure->variable.off + field->offset + offset);
            break;
        case AST_TYPE_VAR_GLOBAL:
            gen_load_global(field, structure->variable.name, field->offset + offset);
            break;
        case AST_TYPE_STRUCT:
            gen_load_structure(structure->structure, field, structure->ctype->offset + offset);
            break;
        case AST_TYPE_DEREFERENCE:
            gen_expression(structure->unary.operand);
            gen_load_local(field, "rax", field->offset + offset);
            break;
        default:
            compile_error("Internal error: gen_assignment_structure");
            break;
    }
}

static void gen_assignment(ast_t *var) {
    switch (var->type) {
        case AST_TYPE_DEREFERENCE:
            gen_assignment_dereference(var);
            break;
        case AST_TYPE_STRUCT:
            gen_assignment_structure(var->structure, var->ctype, 0);
            break;
        case AST_TYPE_VAR_LOCAL:
            gen_ensure_lva(var);
            gen_save_local(var->ctype, var->variable.off);
            break;
        case AST_TYPE_VAR_GLOBAL:
            gen_save_global(var->variable.name, var->ctype, 0);
            break;
        default:
            compile_error("Internal error: gen_assignment");
    }
}

static void gen_comparision(char *operation, ast_t *ast) {
    if (ast_type_floating(ast->left->ctype) || ast_type_floating(ast->right->ctype)) {
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
        gen_emit("cmp %%rax, %rcx");
    }
    gen_emit("%s %%al", operation);
    gen_emit("movzb %%al, %%eax");
}

static void gen_binary_arithmetic_integer(ast_t *ast) {
    char *op;
    switch (ast->type) {
        case '+':             op = "add";  break;
        case '-':             op = "sub";  break;
        case '*':             op = "imul"; break;
        case '^':             op = "xor";  break;
        case AST_TYPE_LSHIFT: op = "sal";  break;
        case AST_TYPE_RSHIFT: op = "sar";  break;
        case '/':
        case '%':
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

    if (ast->type == '/' || ast->type == '%') {
        gen_emit("cqto");
        gen_emit("idiv %%rcx");
        if (ast->type == '%')
            gen_emit("mov %%edx, %%eax");
    } else if (ast->type == AST_TYPE_LSHIFT || ast->type == AST_TYPE_RSHIFT) {
        gen_emit("%s %%cl, %%rax", op);
    } else {
        gen_emit("%s %%rcx, %%rax", op);
    }
}

static void gen_binary_arithmetic_floating(ast_t *ast) {
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
}

static void gen_load(data_type_t *to, data_type_t *from) {
    if (ast_type_floating(to))
        gen_cast_float(from);
    else
        gen_cast_int(from);
}

static void gen_save(data_type_t *to, data_type_t *from) {
    if (ast_type_integer(from) && to->type == TYPE_FLOAT)
        gen_emit("cvtsi2ss %%eax, %%xmm0");
    else if (ast_type_floating(from) && to->type == TYPE_FLOAT)
        gen_emit("cvtpd2ps %%xmm0, %%xmm0");
    else if (ast_type_integer(from) && (to->type == TYPE_DOUBLE || to->type == TYPE_LDOUBLE))
        gen_emit("cvtsi2sd %%eax, %%xmm0");
    else if (!(ast_type_floating(from) && (to->type == TYPE_DOUBLE || to->type == TYPE_LDOUBLE)))
        gen_load(to, from);
}

static void gen_binary(ast_t *ast) {
    if (ast->ctype->type == TYPE_POINTER) {
        gen_pointer_arithmetic(ast->type, ast->left, ast->right);
        return;
    }

    switch (ast->type) {
        case '<':             gen_comparision("setl",  ast); return;
        case '>':             gen_comparision("setg",  ast); return;
        case AST_TYPE_EQUAL:  gen_comparision("sete",  ast); return;
        case AST_TYPE_GEQUAL: gen_comparision("setge", ast); return;
        case AST_TYPE_LEQUAL: gen_comparision("setle", ast); return;
        case AST_TYPE_NEQUAL: gen_comparision("setne", ast); return;
    }

    if (ast_type_integer(ast->ctype))
        gen_binary_arithmetic_integer(ast);
    else if (ast_type_floating(ast->ctype))
        gen_binary_arithmetic_floating(ast);
    else
        compile_error("Internal error");
}

static void gen_literal_save(ast_t *ast, data_type_t *type, int offset) {
    switch (type->type) {
        case TYPE_CHAR:  gen_emit("movb $%d, %d(%%rbp)", ast->integer, offset); break;
        case TYPE_SHORT: gen_emit("movw $%d, %d(%%rbp)", ast->integer, offset); break;
        case TYPE_INT:   gen_emit("movl $%d, %d(%%rbp)", ast->integer, offset); break;

        case TYPE_LONG:
        case TYPE_LLONG:
        case TYPE_POINTER:
            gen_push("rax");
            gen_emit("movq $%lu, %%rax", (unsigned long)ast->integer);
            gen_emit("movq %%rax, %d(%%rbp)", offset);
            gen_pop("rax");
            break;

        case TYPE_FLOAT:
        case TYPE_DOUBLE:
            gen_push("rax");
            gen_emit("movq $%uld, %%rax", *(long*)&ast->floating);
            gen_emit("movq %%rax, %d(%%rbp)", offset);
            gen_pop("rax");
            break;

        default:
            compile_error("codegen internal error in %s", __func__);
    }
}

static void gen_declaration_initialization(list_t *init, int offset) {
    for (list_iterator_t *it = list_iterator(init); !list_iterator_end(it); ) {
        ast_t *node = list_iterator_next(it);
        if (node->init.value->type == AST_TYPE_LITERAL)
            gen_literal_save(node->init.value, node->init.type, node->init.offset + offset);
        else {
            gen_expression(node->init.value);
            gen_save_local(node->init.type, node->init.offset + offset);
        }
    }
}

static void gen_emit_prefix(ast_t *ast, const char *op) {
    gen_expression(ast->unary.operand);
    gen_emit("%s $1, %%rax", op);
    gen_assignment(ast->unary.operand);
}

static void gen_emit_postfix(ast_t *ast, const char *op) {
    gen_expression(ast->unary.operand);
    gen_push("rax");
    gen_emit("%s $1, %%rax", op);
    gen_assignment(ast->unary.operand);
    gen_pop("rax");
}

static list_t *gen_function_argument_types(ast_t *ast) {
    list_t *list = list_create();
    for (list_iterator_t *it = list_iterator(ast->function.call.args),
                         *jt = list_iterator(ast->function.call.paramtypes); !list_iterator_end(it); ) {

        ast_t       *value = list_iterator_next(it);
        data_type_t *type  = list_iterator_next(jt);

        list_push(list, type ? type : ast_result_type('=', value->ctype, ast_data_table[AST_DATA_INT]));
    }
    return list;
}

static void gen_je(const char *label) {
    gen_emit("test %%rax, %%rax");
    gen_emit("je %s", label);
}

static void gen_label(const char *label) {
    gen_emit("%s:", label);
}

static void gen_jmp(const char *label) {
    gen_emit("jmp %s", label);
}

static void gen_expression(ast_t *ast) {
    if (!ast) return;

    char *begin;
    char *ne;
    char *end;
    char *step;
    char *skip;

    int regi = 0, backi;
    int regx = 0, backx;

    list_t *argtypes;

    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_CHAR:
                    gen_emit("mov $%d, %%rax", ast->integer);
                    break;

                case TYPE_INT:
                    gen_emit("mov $%d, %%rax", ast->integer);
                    break;

                case TYPE_LONG:
                case TYPE_LLONG:
                    gen_emit("mov $%lu, %%rax", (unsigned long)ast->integer);
                    break;

                case TYPE_FLOAT:
                case TYPE_DOUBLE:
                case TYPE_LDOUBLE:
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
            gen_ensure_lva(ast);
            gen_load_local(ast->ctype, "rbp", ast->variable.off);
            break;
        case AST_TYPE_VAR_GLOBAL:
            gen_load_global(ast->ctype, ast->variable.label, 0);
            break;

        case AST_TYPE_CALL:
            argtypes = gen_function_argument_types(ast);
            for (list_iterator_t *it = list_iterator(argtypes); !list_iterator_end(it); ) {
                if (ast_type_floating(list_iterator_next(it))) {
                    if (regx > 0) gen_push_xmm(regx);
                    regx++;
                } else {
                    gen_push(registers[regi++]);
                }
            }

            for (list_iterator_t *it = list_iterator(ast->function.call.args),
                                 *jt = list_iterator(argtypes); !list_iterator_end(it); )
            {
                ast_t *v = list_iterator_next(it);
                gen_expression(v);
                data_type_t *ptype = list_iterator_next(jt);
                gen_save(ptype, v->ctype);
                if (ast_type_floating(ptype))
                    gen_push_xmm(0);
                else
                    gen_push("rax");
            }

            backi = regi;
            backx = regx;

            for (list_iterator_t *it = list_iterator(list_reverse(argtypes)); !list_iterator_end(it); ) {
                if (ast_type_floating(list_iterator_next(it))) {
                    gen_pop_xmm(--backx);
                } else {
                    gen_pop(registers[--backi]);
                }
            }
            gen_emit("mov $%d, %%eax", regx);
            if (gen_stack % 16)
                gen_emit("sub $8, %%rsp");

            gen_emit("call %s", ast->function.name);

            if (gen_stack % 16)
               gen_emit("add $8, %%rsp");


            for (list_iterator_t *it = list_iterator(list_reverse(argtypes)); !list_iterator_end(it); ) {
                if (ast_type_floating(list_iterator_next(it))) {
                    if (regx != 1)
                        gen_pop_xmm(--regx);
                } else {
                    gen_pop(registers[--regi]);
                }
            }
            if (ast->ctype->type == TYPE_FLOAT)
                gen_emit("cvtps2pd %%xmm0, %%xmm0");
            break;

        case AST_TYPE_DECLARATION:
            if (ast->decl.init)
                gen_declaration_initialization(ast->decl.init, ast->decl.var->variable.off);
            break;

        case AST_TYPE_ADDRESS:
            switch (ast->unary.operand->type) {
                case AST_TYPE_VAR_LOCAL:
                    gen_ensure_lva(ast->unary.operand);
                    gen_emit("lea %d(%%rbp), %%rax", ast->unary.operand->variable.off);
                    break;

                case AST_TYPE_VAR_GLOBAL:
                    gen_emit("lea %s(%%rip), %%rax", ast->unary.operand->variable.label);
                    break;

                case AST_TYPE_DEREFERENCE:
                    gen_expression(ast->unary.operand);
                    break;

                default:
                    compile_error("Internal error");
                    break;
            }
            break;

        case AST_TYPE_DEREFERENCE:
            gen_expression(ast->unary.operand);
            gen_load_local(ast->unary.operand->ctype->pointer, "rax", 0);
            gen_load(ast->ctype, ast->unary.operand->ctype->pointer);
            break;

        case AST_TYPE_STATEMENT_IF:
        case AST_TYPE_EXPRESSION_TERNARY:
            gen_expression(ast->ifstmt.cond);
            ne = ast_label();
            gen_je(ne);
            gen_expression(ast->ifstmt.then);
            if (ast->ifstmt.last) {
                end = ast_label();
                gen_jmp(end);
                gen_label(ne);
                gen_expression(ast->ifstmt.last);
                gen_label(end);
            } else {
                gen_label(ne);
            }
            break;

        case AST_TYPE_STATEMENT_FOR:
            if (ast->forstmt.init)
                gen_expression(ast->forstmt.init);
            begin = ast_label();
            step  = ast_label();
            end   = ast_label();
            gen_jump_save(end, step);
            gen_label(begin);
            if (ast->forstmt.cond) {
                gen_expression(ast->forstmt.cond);
                gen_je(end);
            }
            gen_expression(ast->forstmt.body);
            gen_label(step);
            if (ast->forstmt.step)
                gen_expression(ast->forstmt.step);
            gen_jmp(begin);
            gen_label(end);
            gen_jump_restore();
            break;

        case AST_TYPE_STATEMENT_WHILE:
            begin = ast_label();
            end   = ast_label();
            gen_jump_save(end, begin);
            gen_label(begin);
            gen_expression(ast->forstmt.cond);
            gen_je(end);
            gen_expression(ast->forstmt.body);
            gen_jmp(begin);
            gen_label(end);
            gen_jump_restore();
            break;

        case AST_TYPE_STATEMENT_DO:
            begin = ast_label();
            end   = ast_label();
            gen_jump_save(end, begin);
            gen_label(begin);
            gen_expression(ast->forstmt.body);
            gen_expression(ast->forstmt.cond);
            gen_je(end);
            gen_jmp(begin);
            gen_label(end);
            gen_jump_restore();
            break;

        case AST_TYPE_STATEMENT_BREAK:
            if (!gen_label_break)
                compile_error("ICE");
            gen_jmp(gen_label_break);
            break;

        case AST_TYPE_STATEMENT_CONTINUE:
            if (!gen_label_continue)
                compile_error("ICE");
            gen_jmp(gen_label_continue);
            break;

        case AST_TYPE_STATEMENT_SWITCH:
            gen_label_switch_store = gen_label_switch;
            gen_label_break_store  = gen_label_break;
            gen_expression(ast->switchstmt.expr);
            gen_label_switch = ast_label();
            gen_label_break  = ast_label();
            gen_jmp(gen_label_switch);
            gen_expression(ast->switchstmt.body);
            gen_label(gen_label_switch);
            gen_label(gen_label_break);
            gen_label_switch = gen_label_switch_store;
            gen_label_break  = gen_label_break_store;
            break;

        case AST_TYPE_STATEMENT_CASE:
            if (!gen_label_switch)
                compile_error("ICE");
            skip = ast_label();
            gen_jmp(skip);
            gen_label(gen_label_switch);
            gen_emit("cmp $%d, %%eax", ast->casevalue);
            gen_label_switch = ast_label();
            gen_emit("jne %s", gen_label_switch);
            gen_label(skip);
            break;

        case AST_TYPE_STATEMENT_DEFAULT:
            if (!gen_label_switch)
                compile_error("ICE");
            gen_label(gen_label_switch);
            gen_label_switch = ast_label();
            break;

        case AST_TYPE_STATEMENT_GOTO:
            gen_jmp(ast->gotostmt.where);
            break;

        case AST_TYPE_STATEMENT_LABEL:
            if (ast->gotostmt.where)
                gen_label(ast->gotostmt.where);
            break;

        case AST_TYPE_STATEMENT_RETURN:
            if (ast->returnstmt) {
                gen_expression(ast->returnstmt);
                gen_save(ast->ctype, ast->returnstmt->ctype);
            }
            gen_emit("leave");
            gen_emit("ret");
            break;

        case AST_TYPE_STATEMENT_COMPOUND:
            for (list_iterator_t *it = list_iterator(ast->compound); !list_iterator_end(it); )
                gen_expression(list_iterator_next(it));
            break;

        case AST_TYPE_STRUCT:
            gen_load_structure(ast->structure, ast->ctype, 0);
            break;

        case '!':
            gen_expression(ast->unary.operand);
            gen_emit("cmp $0, %%rax");
            gen_emit("sete %%al");
            gen_emit("movzb %%al, %%eax");
            break;

        case AST_TYPE_AND:
            end = ast_label();
            gen_expression(ast->left);
            gen_emit("test %%rax, %%rax");
            gen_emit("mov $0, %%rax");
            gen_emit("je %s", end);
            gen_expression(ast->right);
            gen_emit("test %%rax, %%rax");
            gen_emit("mov $0, %%rax");
            gen_emit("je %s", end);
            gen_emit("mov $1, %%rax");
            gen_label(end);
            break;

        case AST_TYPE_OR:
            end = ast_label();
            gen_expression(ast->left);
            gen_emit("test %%rax, %%rax");
            gen_emit("mov $1, %%rax");
            gen_emit("jne %s", end);
            gen_expression(ast->right);
            gen_emit("test %%rax, %%rax");
            gen_emit("mov $1, %%rax");
            gen_emit("jne %s", end);
            gen_emit("mov $0, %%rax");
            gen_label(end);
            break;

        case '&':
        case '|':
            gen_expression(ast->left);
            gen_push("rax");
            gen_expression(ast->right);
            gen_pop("rcx");
            gen_emit("%s %%rcx, %%rax", (ast->type == '|') ? "or" : "and");
            break;

        case '~':
            gen_expression(ast->left);
            gen_emit("not %%rax");
            break;

        case AST_TYPE_POST_INCREMENT: gen_emit_postfix(ast, "add"); break;
        case AST_TYPE_POST_DECREMENT: gen_emit_postfix(ast, "sub"); break;
        case AST_TYPE_PRE_INCREMENT:  gen_emit_prefix (ast, "add"); break;
        case AST_TYPE_PRE_DECREMENT:  gen_emit_prefix (ast, "sub"); break;

        case AST_TYPE_EXPRESSION_CAST:
            gen_expression(ast->unary.operand);
            gen_load(ast->ctype, ast->unary.operand->ctype);
            break;

        case '=':
            gen_expression(ast->right);
            gen_load(ast->ctype, ast->right->ctype);
            gen_assignment(ast->left);
            break;

        default:
            gen_binary(ast);
    }
}

int parse_evaluate(ast_t *ast);
static void gen_data_initialization_intermediate(table_t *labels, char *data, table_t *literal, list_t *init, int offset) {
    for (list_iterator_t *it = list_iterator(init); !list_iterator_end(it); ) {
        ast_t *node = list_iterator_next(it);

        if (node->init.value->type                == AST_TYPE_ADDRESS
        &&  node->init.value->unary.operand->type == AST_TYPE_VAR_LOCAL
        &&  node->init.value->unary.operand->variable.init) {

            char     *label  = ast_label();
            string_t *string = string_create();

            string_catf(string, "%d", node->init.offset + offset);

            table_insert(literal, label, node->init.value->unary.operand);
            table_insert(labels,  string_buffer(string), label);
            continue;
        }

        if (node->init.value->type == AST_TYPE_VAR_LOCAL && node->init.value->variable.init) {
            gen_data_initialization_intermediate(labels, data, literal, node->init.value->variable.init, node->init.offset + offset);
            continue;
        }

        switch (node->init.type->type) {
            case TYPE_FLOAT:
                *(float*)(data + node->init.offset + offset) = node->init.value->floating.value;
                break;

            case TYPE_DOUBLE:
                *(double*)(data + node->init.offset + offset) = node->init.value->floating.value;
                break;

            case TYPE_CHAR:
                *(char*)(data + node->init.offset + offset) = parse_evaluate(node->init.value);
                break;

            case TYPE_SHORT:
                *(short*)(data + node->init.offset + offset) = parse_evaluate(node->init.value);
                break;

            case TYPE_INT:
                *(int*)(data + node->init.offset + offset) = parse_evaluate(node->init.value);
                break;

            case TYPE_LONG:
                *(long*)(data + node->init.offset + offset) = parse_evaluate(node->init.value);
                break;

            case TYPE_LLONG:
                *(long long*)(data + node->init.offset + offset) = parse_evaluate(node->init.value);
                break;

            case TYPE_POINTER:
                *(long *)(data + node->init.offset + offset) = parse_evaluate(node->init.value);

            default:
                break;
        }
    }
}

static void gen_data_initialization(table_t *table, list_t *list, int size) {
    char *data = memory_allocate(size);
    memset(data, 0, size);

    table_t *labels = table_create(NULL);
    gen_data_initialization_intermediate(labels, data, table, list, 0);

    int i = 0;
    for (; i <= size - 4; i += 4) {
        string_t *string = string_create();
        string_catf(string, "%d", i);
        char *label = table_find(labels, string_buffer(string));
        if (label) {
            gen_emit(".quad %s", label);
            i += 4;
        } else {
            gen_emit(".long %d", data[i]);
        }
    }
    for (; i < size; i++)
        gen_emit(".byte %d", data[i]);
    gen_emit(".align 8");
}

/*
 * Recursive compliteral generation, emits data initialization
 * until there is nothing to initialize left.
 */
static void gen_data_literal(char *label, ast_t *ast) {
    table_t *table = table_create(NULL);
    gen_emit_inline("%s:", label);
    gen_data_initialization(table, ast->variable.init, ast->ctype->size);

    for (list_iterator_t *it = list_iterator(table_keys(table)); !list_iterator_end(it); ) {
        char  *label = list_iterator_next(it);
        ast_t *node  = table_find(table, label);

        gen_data_literal(label, node);
    }
}

static void gen_data(ast_t *ast) {
    table_t *table = table_create(NULL);

    if (!ast->decl.var->ctype->isstatic)
        gen_emit_inline(".global %s", ast->decl.var->variable.name);

    gen_emit_inline("%s:", ast->decl.var->variable.name);
    gen_data_initialization(table, ast->decl.init, ast->decl.var->ctype->size);

    for (list_iterator_t *it = list_iterator(table_keys(table)); !list_iterator_end(it); ) {
        char  *label = list_iterator_next(it);
        ast_t *node  = table_find(table, label);

        gen_data_literal(label, node);
    }
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

void gen_data_section(void) {
    gen_emit(".data");

    for (list_iterator_t *it = list_iterator(ast_strings); !list_iterator_end(it); ) {
        ast_t *ast = list_iterator_next(it);
        gen_emit_inline("%s: ", ast->string.label);
        gen_emit(".string \"%s\"", string_quote(ast->string.data));
    }

    for (list_iterator_t *it = list_iterator(ast_floats); !list_iterator_end(it); ) {
        ast_t *ast   = list_iterator_next(it);
        char  *label = ast_label();

        ast->floating.label = label;
        gen_emit_inline("%s:", label);
        gen_emit(".long %d", ((int*)&ast->floating.value)[0]);
        gen_emit(".long %d", ((int*)&ast->floating.value)[1]);
    }
}

static int gen_alignment(int n, int align) {
    int remainder = n % align;
    return (remainder == 0)
                ? n
                : n - remainder + align;
}

static void gen_function_prologue(ast_t *ast) {
    if (list_length(ast->function.params) > sizeof(registers)/sizeof(registers[0]))
        compile_error("Too many params for function");

    gen_emit_inline(".text");
    gen_emit_inline(".global %s", ast->function.name);
    gen_emit_inline("%s:", ast->function.name);
    gen_push("rbp");
    gen_emit("mov %%rsp, %%rbp");

    int offset = 0;
    int regi   = 0;
    int regx   = 0;

    for (list_iterator_t *it = list_iterator(ast->function.params); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);

        if (value->ctype->type == TYPE_FLOAT) {
            gen_push_xmm(regx++);
        } else if (value->ctype->type == TYPE_DOUBLE|| value->ctype->type == TYPE_LDOUBLE) {
            gen_push_xmm(regx++);
        } else {
            gen_push(registers[regi++]);
        }
        offset -= gen_alignment(value->ctype->size, 8);
        value->variable.off = offset;
    }

    int localdata = 0;
    for (list_iterator_t *it = list_iterator(ast->function.locals); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);
        offset -= gen_alignment(value->ctype->size, 8);
        value->variable.off = offset;
        localdata += offset;
    }

    if (localdata)
        gen_emit("sub $%d, %%rsp", -localdata);
    gen_stack += -(offset - 8);
}

static void gen_function_epilogue(void) {
    gen_emit("leave");
    gen_emit("ret");
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
        compile_error("ICE");
    }
    if (gen_stack > 8)
        printf("## stack is misaligned by %d (bytes)\n", gen_stack);
}
