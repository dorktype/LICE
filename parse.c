#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "gmcc.h"

#define PARSE_BUFFER 1024
#define PARSE_CALLS  6    // only six registers to use for amd64

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

char *parse_identifier(int c) {
    char  *buffer = malloc(PARSE_BUFFER);
    int    i      = 1;

    buffer[0] = c;
    for (;;) {
        int c = getc(stdin);
        if (!isalnum(c)) {
            ungetc(c, stdin);
            break;
        }

        buffer[i++] = c;
        if (i == PARSE_BUFFER - 1)
            compile_error("Internal error: OOM");
    }

    buffer[i] = '\0';
    return buffer;
}

ast_t *parse_expression(size_t lastpri);
ast_t *parse_function_call(char *name) {
    ast_t **args = (ast_t**)malloc(sizeof(ast_t*) * (PARSE_CALLS + 1));
    size_t  i;
    char    c;

    size_t  wrote; // how many wrote arguments

    for (i = 0; i < PARSE_CALLS; i++) {
        parse_skip();

        // break when call close
        if ((c = getc(stdin)) == ')')
            break;

        ungetc(c, stdin);
        args[i] = parse_expression(0);
        wrote++;

        // check for here as well
        if ((c = getc(stdin)) == ')')
            break;

        // skip spaces to next argument
        if (c == ',')
            parse_skip();
        else
            compile_error("Unexpected character in function call");
    }

    if (i == PARSE_CALLS)
        compile_error("too many arguments");

    // now build the node! :D
    return ast_new_func_call(name, wrote, args);
}

// parse generic handles identifiers for variables
// or function calls, it's generic because it needs
// to determine which is an ident or function call
// than dispatch the correct parse routine
ast_t *parse_generic(char c1) {
    char  c2;
    char  *name = parse_identifier(c1);
    var_t *var;

    parse_skip();

    // it's a funciton call?
    if ((c2 = getc(stdin)) == '(')
        return parse_function_call(name);

    ungetc(c2, stdin);

    // check for variable
    if (!(var = var_find(name)))
        var = var_create(name);

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
        return parse_generic(c);
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
