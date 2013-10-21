#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "gmcc.h"

#define PARSE_BUFFER 1024

static void parse_skip(void) {
    int c;
    while ((c = getc(stdin)) != EOF) {
        if (isspace(c))
            continue;
        ungetc(c, stdin);
        return;
    }
}

// what priority does the following operator get?
static int parse_operator_priority(char operator) {
    switch (operator) {
        case '=':           return 1;
        case '+': case '-': return 2;
        case '*': case '/': return 3;
    }
    return -1;
}

ast_t *parse_symbol(int c) {
    var_t *var;
    char  *buffer = malloc(PARSE_BUFFER);
    int    i      = 1;

    buffer[0] = c;
    for (;;) {
        int c = getc(stdin);
        if (!isalpha(c)) {
            ungetc(c, stdin);
            break;
        }

        buffer[i++] = c;
        if (i == PARSE_BUFFER - 1)
            compile_error("Internal error: OOM");
    }

    buffer[i] = '\0';
    if (!(var = var_find(buffer)))
        var = var_create(buffer);

    return ast_new_data_var(var);
}

ast_t *parse_integer(int value) {
    for (;;) {
        int c = getc(stdin);
        if (!isdigit(c)) {
            ungetc(c, stdin);
            return ast_new_data_int(value);
        }
        value = value * 10 + (c - '0');
    }
    return NULL;
}

ast_t *parse_expression_primary(void) {
    int c = getc(stdin);
    if (isdigit(c))
        return parse_integer(c - '0');
    if (isalpha(c))
        return parse_symbol(c);
    else if (c == EOF)
        return NULL;

    compile_error("Syntax error: %c", c);
    return NULL;
}

// handles operator precedence climbing as well
ast_t *parse_expression(size_t lastpri) {
    ast_t *ast;
    int    pri;

    parse_skip();

    // no primary expression?
    if (!(ast = parse_expression_primary()))
        return NULL;

    for (;;) {
        int c;
        parse_skip();

        if ((c = getc(stdin)) == EOF)
            return ast;

        // operator precedence handling
        pri = parse_operator_priority(c);

        // (size_t)-1 on error, test that first
        // hence the integer cast
        if (pri < 0 || pri < lastpri) {
            ungetc(c, stdin);
            return ast;
        }

        parse_skip();

        // precedence climbing is way too easy with recursive
        // descent parsing, thanks q66 for showing me this.
        ast = ast_new_bin_op(c, ast, parse_expression(pri + 1));
    }
    return ast;
}

static ast_t *parse_dopass(void) {
    // recursive descent with zero
    ast_t *ast;
    int    c;

    if (!(ast = parse_expression(0)))
        return NULL;

    // deal with expression termination
    parse_skip();
    if ((c = getc(stdin)) != ';')
        compile_error("Expected semicolon to terminate expression %c", c);

    return ast;
}

void parse_compile(FILE *as, int dump) {
    ast_t *ast;

    // emit the entry
    if (!dump) {
        fprintf(
            as,"\
            .text\n\
            .global " GMCC_ENTRY "\n"
            GMCC_ENTRY ":\n"
        );
    }

    for (;;) {
        if (!(ast = parse_dopass()))
            break;
        if (dump)
            ast_dump(ast);
        else
            gen_emit_expression(as, ast);
    }
    if (!dump)
        fprintf(as, "ret\n");
}
