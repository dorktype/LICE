#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "gmcc.h"

#define PARSE_BUFFER 1024
#define PARSE_CALLS  6    // only six registers to use for amd64

// what priority does the following operator get?
static int parse_operator_priority(char operator) {
    switch (operator) {
        case '=':           return 1;
        case '+': case '-': return 2;
        case '*': case '/': return 3;
    }
    return -1;
}

static ast_t *parse_expression(int lastpri);

static ast_t *parse_function_call(char *name) {
    ast_t **args = (ast_t**)malloc(sizeof(ast_t*) * (PARSE_CALLS + 1));
    size_t  i;
    size_t  wrote = 0; // how many wrote arguments

    for (i = 0; i < PARSE_CALLS; i++) {

        // break when call is done
        lexer_token_t *token = lexer_next();
        if (lexer_ispunc(token, ')'))
            break;
        lexer_unget(token);

        args[i] = parse_expression(0);
        wrote++;

        // deal with call done here as well
        token = lexer_next();
        if (lexer_ispunc(token, ')'))
            break;
        if (!lexer_ispunc(token, ','))
            compile_error("Unexpected character in function call");
    }

    if (i == PARSE_CALLS)
        compile_error("too many arguments");

    // now build the node! :D
    return ast_new_func_call(name, wrote, args);
}


static ast_t *parse_generic(char *name) {
    ast_t         *var   = NULL;
    lexer_token_t *token = lexer_next();

    if (lexer_ispunc(token, '('))
        return parse_function_call(name);

    lexer_unget(token);

    if (!(var = var_find(name)))
        compile_error("Undefined variable: %s", name);

    return var;
}

static ast_t *parse_expression_primary(void) {
    lexer_token_t *token;

    if (!(token = lexer_next()))
        return NULL;

    switch (token->type) {
        case LEXER_TOKEN_IDENT:
            return parse_generic(token->string);

        // ast generating ones
        case LEXER_TOKEN_INT:    return ast_new_int(token->integer);
        case LEXER_TOKEN_CHAR:   return ast_new_char(token->character);
        case LEXER_TOKEN_STRING: return ast_new_string(token->string);

        case LEXER_TOKEN_PUNCT:
            compile_error("Unexpected punctuation: `%c`", token->punct);
            return NULL;
    }
    compile_error("Internal error: Expected token");
    return NULL;
}

static data_type_t *parse_semantic_result_impl(jmp_buf *jmpbuf, char op, data_type_t *a, data_type_t *b) {
    if (a->type > b->type) {
        data_type_t *t = a;
        a = b;
        b = t;
    }

    if (b->type == TYPE_PTR) {
        if (op != '+' && op != '-')
            goto error;

        if (a->type != TYPE_PTR) {
            // warning
            return b;
        }

        data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
        data->type        = TYPE_PTR;
        data->pointer     = parse_semantic_result_impl(jmpbuf, op, a->pointer, b->pointer);
        return data;
    }


    switch (a->type) {
        case TYPE_VOID:
            goto error;
        case TYPE_INT:
        case TYPE_CHAR:
            return ast_data_int();
        case TYPE_ARRAY:
            return parse_semantic_result_impl(jmpbuf, op, ast_new_pointer(a->pointer), b);
        default:
            compile_error("Internal error");
    }

error:
    longjmp(*jmpbuf, 1);

    return NULL;
}

static data_type_t *parse_semantic_result(char op, ast_t *a, ast_t *b) {
    jmp_buf jmpbuf;
    if (setjmp(jmpbuf) == 0)
        return parse_semantic_result_impl(&jmpbuf, op, a->ctype, b->ctype);

    compile_error("Incompatible types in expression: <%s> %c <%s>",
        op,
        ast_dump_string(a),
        ast_dump_string(b)
    );

    return NULL;
}

// parser semantic enforcers
static void parse_semantic_lvalue(ast_t *ast) {
    // enforce lvalue semantics
    if (ast->type != ast_type_data_var)
        compile_error("Expected lvalue");
}

static bool parse_semantic_rightassoc(char operator) {
    // enforce right associative semantics
    return operator == '=';
}

static ast_t *parse_expression_unary(void) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunc(token, '&')) {
        ast_t *operand = parse_expression_unary();
        parse_semantic_lvalue(operand);
        return ast_new_unary(ast_type_addr, ast_new_pointer(operand->ctype), operand);
    }

    if (lexer_ispunc(token, '*')) {
        ast_t *operand = parse_expression_unary();
        if (operand->ctype->type != TYPE_PTR)
            compile_error("Expected pointer type, cannot dereference expression <%s>", ast_dump_string(operand));
        return ast_new_unary(ast_type_deref, operand->ctype->pointer, operand);
    }

    lexer_unget(token);
    return parse_expression_primary();
}

// handles operator precedence climbing as well
static ast_t *parse_expression(int lastpri) {
    ast_t       *ast;
    ast_t       *next;
    data_type_t *data;

    // no primary expression?
    if (!(ast = parse_expression_unary()))
        return NULL;

    for (;;) {
        lexer_token_t *token = lexer_next();
        if (token->type != LEXER_TOKEN_PUNCT) {
            lexer_unget(token);
            return ast;
        }

        int pri = parse_operator_priority(token->punct);
        if (pri < 0 || pri < lastpri) {
            lexer_unget(token);
            return ast;
        }

        if (lexer_ispunc(token, '='))
            parse_semantic_lvalue(ast);

        next = parse_expression(pri + !parse_semantic_rightassoc(token->punct));
        data = parse_semantic_result(token->punct, ast, next);

        // swap
        if (data->type == TYPE_PTR && ast->ctype->type != TYPE_PTR) {
            ast_t *t = ast;
            ast  = next;
            next = t;
        }

        ast  = ast_new_binary(token->punct, data, ast, next);
    }
    return NULL;
}

static data_type_t *parse_type_get(lexer_token_t *token) {
    if (token->type != LEXER_TOKEN_IDENT)
        return NULL;

    if (!strcmp(token->string, "int"))
        return ast_data_int();
    if (!strcmp(token->string, "char"))
        return ast_data_char();

    return NULL;
}

static bool parse_type_check(lexer_token_t *token) {
    return parse_type_get(token) != NULL;
}

static void parse_expect(char punct) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunc(token, punct))
        compile_error("Expected `%c`, got %s instead", punct, lexer_tokenstr(token));
}


static ast_t *parse_declaration(void) {
    lexer_token_t *token;
    ast_t         *var;
    data_type_t   *type = parse_type_get(lexer_next());

    for (;;) {
        token = lexer_next();
        if (!lexer_ispunc(token, '*'))
            break;

        type = ast_new_pointer(type);
    }

    if (token->type != LEXER_TOKEN_IDENT)
        compile_error("Expected identifier, got `%s` instead", lexer_tokenstr(token));

    var = ast_new_variable(type, token->string);

    // expect for init
    parse_expect('=');

    return ast_new_decl(var, parse_expression(0));
}

static ast_t *parse_statement(void) {
    lexer_token_t *token = lexer_peek();
    ast_t         *ast;

    if (!token)
        return NULL;

    ast = parse_type_check(token)
            ? parse_declaration()
            : parse_expression(0);

    token = lexer_next();
    if (!lexer_ispunc(token, ';'))
        compile_error("Expected termination of expression: %s", lexer_tokenstr(token));

    return ast;
}


// compile stage
void parse_compile(FILE *as, int dump) {
    ast_t *ast[1024];
    int    i;
    int    n;

    for (i = 0; i < 1024; i++) {
        ast_t *load = parse_statement();
        if   (!load) break;
        ast[i] = load;
    }

    // emit the entry
    if (!dump) {
        gen_emit_data(as, ast_strings());
        fprintf(
            as,"\
            .text\n\
            .global " GMCC_ENTRY "\n"
            GMCC_ENTRY ":\n\
                push %%rbp\n\
                mov %%rsp, %%rbp\n"
        );

        if (ast_variables())
            fprintf(as, "sub $%d, %%rsp\n", ast_variables()->variable.placement * 8);
    }

    for (n = 0; n < i; n++) {
        if (dump)
            printf("%s", ast_dump_string(ast[n]));
        else
            gen_emit_expression(as, ast[n]);
    }

    if (!dump) {
        fprintf(as, "leave\nret\n");
    }
}
