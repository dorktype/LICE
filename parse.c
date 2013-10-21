#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

static type_t parse_type_get(lexer_token_t *token) {
    if (token->type != LEXER_TOKEN_IDENT)
        return -1;

    if (!strcmp(token->value.string, "void"))
        return TYPE_VOID;
    if (!strcmp(token->value.string, "int"))
        return TYPE_INT;
    if (!strcmp(token->value.string, "char"))
        return TYPE_CHAR;
    if (!strcmp(token->value.string, "string"))
        return TYPE_STR;

    return -1;
}

static bool parse_type_check(lexer_token_t *token) {
    return parse_type_get(token) != -1;
}

static void parse_expect(char punct) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunc(token, punct))
        compile_error("Expected `%c`, got %s instead", punct, lexer_tokenstr(token));
}

static void parse_swap(ast_t *a, ast_t *b) {
    ast_t *t;

    t = a;
    a = b;
    b = t;
}

static ast_t *parse_declaration(void) {
    lexer_token_t *token;
    ast_t         *var;
    type_t        type = parse_type_get(lexer_next());

    token = lexer_next();
    if (token->type != LEXER_TOKEN_IDENT)
        compile_error("Expected identifier, got %s instead", lexer_tokenstr(token));

    var = ast_new_data_var(type, token->value.string);

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


static ast_t *parse_expression_primary(void) {
    lexer_token_t *token;

    if (!(token = lexer_next()))
        return NULL;

    switch (token->type) {
        case LEXER_TOKEN_IDENT:
            return parse_generic(token->value.string);

        // ast generating ones
        case LEXER_TOKEN_INT:    return ast_new_data_int(token->value.integer);
        case LEXER_TOKEN_CHAR:   return ast_new_data_chr(token->value.character);
        case LEXER_TOKEN_STRING: return ast_new_data_str(token->value.string);

        case LEXER_TOKEN_PUNCT:
            compile_error("Unexpected punctuation: `%c`", token->value.punct);
            return NULL;
    }
    compile_error("Internal error: Expected token");
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

static char parse_semantic_result(char operator, ast_t *a, ast_t *b) {
    // enforce result type semantics, e.g type compatabilty
    bool swapped = false;

    if (a->ctype > b->ctype) {
        swapped = true;
        parse_swap(a, b);
    }

    switch (a->ctype) {
        case TYPE_VOID:
            goto parse_semantic_result_error;
        case TYPE_INT:
            switch (b->ctype) {
                case TYPE_INT:
                case TYPE_CHAR:
                    return TYPE_INT;
                case TYPE_STR:
                    goto parse_semantic_result_error;
                default:
                    break;
            }
            compile_error("Internal error");
            break;
        case TYPE_CHAR:
            switch (b->ctype) {
                case TYPE_CHAR:
                    return TYPE_INT;
                case TYPE_STR:
                    goto parse_semantic_result_error;
                default:
                    break;
            }
            compile_error("Internal error");
            break;
        case TYPE_STR:
            goto parse_semantic_result_error;
        default:
            compile_error("Internal error");
    }

parse_semantic_result_error:
    if (swapped)
        parse_swap(a, b);

    compile_error("Incompatible types in operation: %s and %s",
        ast_type_string(a->ctype),
        ast_type_string(b->ctype)
    );
    return -1;
}

// handles operator precedence climbing as well
static ast_t *parse_expression(int lastpri) {
    ast_t *ast;
    ast_t *next;

    // no primary expression?
    if (!(ast = parse_expression_primary()))
        return NULL;

    for (;;) {
        lexer_token_t *token = lexer_next();
        if (token->type != LEXER_TOKEN_PUNCT) {
            lexer_unget(token);
            return ast;
        }

        int pri = parse_operator_priority(token->value.punct);
        if (pri < 0 || pri < lastpri) {
            lexer_unget(token);
            return ast;
        }

        if (lexer_ispunc(token, '='))
            parse_semantic_lvalue(ast);

        // precedence climbing is way too easy with recursive
        // descent parsing, thanks q66 for showing me this.
        next = parse_expression(pri + !parse_semantic_rightassoc(token->value.punct));
        ast  = ast_new_bin_op(
                token->value.punct,
                parse_semantic_result(
                    token->value.punct,
                    ast,
                    next
                ),
                ast,
                next
        );
    }
    return NULL;
}

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
            GMCC_ENTRY ":\n"
        );
    }

    for (n = 0; n < i; n++) {
        if (dump)
            printf("%s", ast_dump_string(ast[n]));
        else
            gen_emit_expression(as, ast[n]);
    }

    if (!dump) {
        fprintf(as, "ret\n");
    }
}
