#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

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

ast_t *parse_expression(size_t lastpri);
ast_t *parse_function_call(char *name) {
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


ast_t *parse_generic(char *name) {
    ast_t         *var;
    lexer_token_t *token = lexer_next();

    if (lexer_ispunc(token, '('))
        return parse_function_call(name);

    lexer_unget(token);

    return ((var = var_find(name)))
                ? var
                : ast_new_data_var(name);
}


ast_t *parse_expression_primary(void) {
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

// handles operator precedence climbing as well
ast_t *parse_expression(size_t lastpri) {
    ast_t *ast;

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

        // precedence climbing is way too easy with recursive
        // descent parsing, thanks q66 for showing me this.
        ast = ast_new_bin_op(token->value.punct, ast, parse_expression(pri + 1));
    }
    return NULL;
}

static ast_t *parse_next(void) {
    // recursive descent with zero
    ast_t         *ast;
    lexer_token_t *token;

    if (!(ast = parse_expression(0)))
        return NULL;

    token = lexer_next();
    if (!lexer_ispunc(token, ';'))
        compile_error("Expected termination in expression: `%s`", lexer_tokenstr(token));

    return ast;
}

void parse_compile(FILE *as, int dump) {
    ast_t *ast[1024];
    int    i;
    int    n;

    for (i = 0; i < 1024; i++) {
        ast_t *load = parse_next();
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
            ast_dump(ast[n]);
        else
            gen_emit_expression(as, ast[n]);
    }

    if (!dump) {
        fprintf(as, "ret\n");
    }
}
