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

#define gen_type_size ast_sizeof

static void gen_load_global(data_type_t *type, char *label, int offset) {
    if (type->type == TYPE_ARRAY) {
        if (offset)
            gen_emit("lea %s+%d(%%rip), %%rax", label, offset);
        else
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

    if (offset)
        gen_emit("mov %s+%d(%%rip), %%%s", label, offset, reg);
    else
        gen_emit("mov %s(%%rip), %%%s", label, reg);
}

static void gen_load_local(data_type_t *var, int offset) {
    if (var->type == TYPE_ARRAY) {
        gen_emit("lea %d(%%rbp), %%rax", offset);
        return;
    }

    int size = gen_type_size(var);
    switch (size) {
        case 1:
            gen_emit("mov $0, %%eax");
            gen_emit("mov %d(%%rbp), %%al", offset);
            break;

        case 4:
            gen_emit("mov %d(%%rbp), %%eax", offset);
            break;

        case 8:
            gen_emit("mov %d(%%rbp), %%rax", offset);
            break;
    }
}

static void gen_save_global(char *name, data_type_t *type, int offset) {
    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
        default:
            compile_error("Internal error");
            break;
    }

    if (offset)
        gen_emit("mov %%%s, %s+%d(%%rip)", reg, name, offset);
    else
        gen_emit("mov %%%s, %s(%%rip)", reg, name);
}

static void gen_save_local(data_type_t *type, int offset) {
    char *reg;
    int size = gen_type_size(type);
    switch (size) {
        case 1: reg = "al";  break;
        case 4: reg = "eax"; break;
        case 8: reg = "rax"; break;
    }
    gen_emit("mov %%%s, %d(%%rbp)", reg, offset);
}

// pointer dereferencing, load and assign
static void gen_pointer_arithmetic(char op, ast_t *left, ast_t *right) {
    gen_expression(left);
    gen_emit("push %%rax");
    gen_expression(right);

    int size = ast_sizeof(left->ctype->pointer);
    if (size > 1)
        gen_emit("imul $%d, %%rax", size);

    gen_emit("mov %%rax, %%rcx");
    gen_emit("pop %%rax");
    gen_emit("add %%rcx, %%rax");
}

static void gen_load_dereference(data_type_t *rtype, data_type_t *otype, int offset) {
    if (otype->type == TYPE_POINTER && otype->pointer->type == TYPE_ARRAY)
        return;

    char *reg;
    switch (ast_sizeof(rtype)) {
        case 1:
            reg = "cl";
            gen_emit("mov $0, %%ecx");
            break;

        case 4: reg = "ecx"; break;
        case 8: reg = "rcx"; break;
    }

    if (offset)
        gen_emit("mov %d(%%rax), %%%s", offset, reg);
    else
        gen_emit("mov (%%rax), %%%s", reg);
    gen_emit("mov %%rcx, %%rax");
}

static void gen_assignment_dereference_intermediate(data_type_t *type, int offset) {
    char *reg;

    gen_emit("pop %%rcx");

    switch (gen_type_size(type)) {
        case 1: reg = "cl";  break;
        case 4: reg = "ecx"; break;
        case 8: reg = "rcx"; break;
    }

    if (offset)
        gen_emit("mov %%%s %d(%%rax)", reg, offset);
    else
        gen_emit("mov %%%s, (%%rax)", reg);
}

static void gen_assignment_dereference(ast_t *var) {
    gen_emit("push %%rax");
    gen_expression(var->unary.operand);
    gen_assignment_dereference_intermediate(var->unary.operand->ctype, 0);
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
            gen_emit("push %%rax");
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
    gen_expression(ast->left);
    gen_emit("push %%rax");
    gen_expression(ast->right);

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

    if (ast->ctype->type == TYPE_POINTER) {
        gen_pointer_arithmetic(ast->type, ast->left, ast->right);
        return;
    }

    char *op;
    switch (ast->type) {
        case LEXER_TOKEN_EQUAL:  gen_comparision("sete",  ast); return;
        case LEXER_TOKEN_GEQUAL: gen_comparision("setge", ast); return;
        case LEXER_TOKEN_LEQUAL: gen_comparision("setle", ast); return;
        case LEXER_TOKEN_NEQUAL: gen_comparision("setne", ast); return;
        case '<':                gen_comparision("setl",  ast); return;
        case '>':                gen_comparision("setg",  ast); return;

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
    gen_emit("push %%rax");
    gen_expression(ast->right);
    gen_emit("mov %%rax, %%rcx");

    if (ast->type == '/') {
        gen_emit("pop %%rax");
        gen_emit("mov $0, %%edx");
        gen_emit("idiv %%rcx");
    } else {
        gen_emit("pop %%rax");
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
    if (list_length(ast_globalenv->variables) == 0)
        return;

    for (list_iterator_t *it = list_iterator(ast_globalenv->variables); !list_iterator_end(it); ) {
        ast_t *ast = list_iterator_next(it);
        if (ast->type == AST_TYPE_STRING) {
            gen_emit_label("%s:", ast->string.label);
            gen_emit_inline(".string \"%s\"", string_quote(ast->string.data));
        } else if (ast->type != AST_TYPE_VAR_GLOBAL) {
            compile_error("TODO: gen_data_section");
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
    gen_emit(".lcomm %s, %d", ast->decl.var->variable.name, gen_type_size(ast->decl.var->ctype));
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
        r -= gen_data_padding(gen_type_size(value->ctype));
        value->variable.off = r;
    }

    for (list_iterator_t *it = list_iterator(ast->function.locals); !list_iterator_end(it); ) {
        ast_t *value = list_iterator_next(it);
        o -= gen_data_padding(gen_type_size(value->ctype));
        value->variable.off = o;
    }

    if (o)
        gen_emit("sub $%d, %%rsp", -o);

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
        compile_error("TODO: gen_function");
    }
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
            gen_load_global(ast->ctype, ast->variable.label, ast->variable.off);
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
                    gen_save_local(ast->decl.var->ctype->pointer, ast->decl.var->variable.off + i);
                    i += ast_sizeof(ast->decl.var->ctype->pointer);
                }
            } else if (ast->decl.var->ctype->type == TYPE_ARRAY) {
                char *p;
                for (i = 0, p = ast->decl.init->string.data; *p; p++, i++)
                    gen_emit("movb $%d, %d(%%rbp)", *p, -(ast->decl.var->variable.off + i));
                gen_emit("movb $0, %d(%%rbp)", -(ast->decl.var->variable.off + i));
            } else if (ast->decl.init->type == AST_TYPE_STRING) {
                gen_load_global(ast->decl.init->ctype, ast->decl.init->string.label, ast->decl.init->variable.off);
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
            gen_emit("push %%rax");
            gen_expression(ast->right);
            gen_emit("pop %%rcx");
            gen_emit("%s %%rcx, %%rax", (ast->type == '|') ? "or" : "and");
            break;

        case LEXER_TOKEN_INCREMENT: gen_emit_postfix(ast, "add"); break;
        case LEXER_TOKEN_DECREMENT: gen_emit_postfix(ast, "sub"); break;

        default:
            gen_binary(ast);
    }
}
