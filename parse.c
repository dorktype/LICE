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
static size_t parse_operator_priority(char operator) {
    switch (operator) {
        case '+':
        case '-':
            return 1;
        case '*':
        case '/':
            return 2;
    }
    return 0;
}

ast_t *parse_integer(int value) {
    int c;
    while ((c = getc(stdin)) != EOF) {
        if (!isdigit(c)) {
            ungetc(c, stdin);
            return ast_new_data_int(value);
        }
        value = value * 10 + (c - '0');
    }
    return NULL;
}

ast_t *parse_string(void) {
    char *buffer = malloc(PARSE_BUFFER);
    int   i = 0;
    int   c;

    for (;;) {
        if ((c = getc(stdin)) == EOF)
            compile_error("Unterminated string");
        if (c == '"')
            break;

        // line continuation
        if (c == '\\') {
            if ((c = getc(stdin)) == EOF)
                compile_error("Unterminated string in line continuation");
        }

        buffer[i++] = c;
        if (i == PARSE_BUFFER - 1)
            compile_error("Internal error: OOM");
    }

    buffer[i] = '\0'; // terminate
    return ast_new_data_str(buffer); // ast will need to free it
}

ast_t *parse_expression_primary(void) {
    int c;
    if (isdigit(c = getc(stdin)))
        return parse_integer(c - '0');
    else if (c == '"')
        return parse_string();
    else if (c == EOF)
        compile_error("Unexpected EOF");
    compile_error("Syntax error");

    return NULL;
}

// handles operator precedence climbing as well
ast_t *parse_expression(size_t lastpri) {
    ast_t *ast = parse_expression_primary();
    size_t pri;

    for (;;) {
        int c;
        parse_skip();

        if ((c = getc(stdin)) == EOF)
            return ast;

        // operator precedence handling
        pri = parse_operator_priority(c);
        if (pri < lastpri) {
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

ast_t *parse(void) {
    // recursive descent with zero
    return parse_expression(0);
}
