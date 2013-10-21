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

ast_t *parse_expression_left(ast_t *left) {
    int c;
    int operation;

    parse_skip();

    if ((c = getc(stdin)) == EOF)
        return left;

    if (c == '+')
        operation = ast_type_bin_add;
    else if (c == '-')
        operation = ast_type_bin_sub;
    else
        compile_error("Expected operator, got `%c` instead", c);

    parse_skip();

    // recursive descent
    return parse_expression_left(
                ast_new_bin_op(
                    operation,
                    left,
                    parse_expression_primary()
                )
            );
}

ast_t *parse(void) {
    // recursive descent
    return parse_expression_left(parse_expression_primary());
}
