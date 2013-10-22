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
    list_t *list = list_create();
    for (;;) {
        // break when call is done
        lexer_token_t *token = lexer_next();
        if (lexer_ispunc(token, ')'))
            break;
        lexer_unget(token);
        list_push(list, parse_expression(0));
        // deal with call done here as well
        token = lexer_next();
        if (lexer_ispunc(token, ')'))
            break;
        if (!lexer_ispunc(token, ','))
            compile_error("Unexpected character in function call");
    }

    if (PARSE_CALLS < list_length(list))
        compile_error("too many arguments");

    return ast_new_call(name, list);
}


static ast_t *parse_generic(char *name) {
    ast_t         *var   = NULL;
    lexer_token_t *token = lexer_next();

    if (lexer_ispunc(token, '('))
        return parse_function_call(name);

    lexer_unget(token);

    if (!(var = ast_find_variable(name)))
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
    compile_error("Internal error %s", __func__);
    return NULL;
}

static data_type_t *parse_semantic_result_impl(jmp_buf *jmpbuf, char op, data_type_t *a, data_type_t *b) {
    if (a->type > b->type) {
        data_type_t *t = a;
        a = b;
        b = t;
    }

    if (b->type == TYPE_PTR) {
        if (op == '=')
            return a;
        if (op != '+' && op != '-')
            goto error;
        if (a->type != TYPE_INT)
            goto error;

        return b;
    }

    switch (a->type) {
        case TYPE_VOID:
            goto error;
        case TYPE_INT:
        case TYPE_CHAR:
            switch (b->type) {
                case TYPE_INT:
                case TYPE_CHAR:
                    return ast_data_int;

                case TYPE_ARRAY:
                case TYPE_PTR:
                    return b;

                case TYPE_VOID:
                    goto error;
            }
            compile_error("Internal error");
            break;

        case TYPE_ARRAY:
            goto error;
        default:
            compile_error("Internal error %s", __func__);
    }

error:
    longjmp(*jmpbuf, 1);

    return NULL;
}

static data_type_t *parse_semantic_result(char op, data_type_t *a, data_type_t *b) {
    jmp_buf jmpbuf;
    if (setjmp(jmpbuf) == 0)
        return parse_semantic_result_impl(&jmpbuf, op, a, b);


    compile_error("Incompatible types in expression");
    return NULL;
}

// parser semantic enforcers
static void parse_semantic_lvalue(ast_t *ast) {
    switch (ast->type) {
        case AST_TYPE_VAR_LOCAL:
        case AST_TYPE_REF_LOCAL:
        case AST_TYPE_VAR_GLOBAL:
        case AST_TYPE_REF_GLOBAL:
        case AST_TYPE_DEREF:
            return;
    }
    compile_error("<%s> isn't a valid lvalue", ast_dump_string(ast));
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
        return ast_new_unary(AST_TYPE_ADDR, ast_new_pointer(operand->ctype), operand);
    }

    if (lexer_ispunc(token, '*')) {
        ast_t *operand = parse_expression_unary();
        if (operand->ctype->type != TYPE_PTR)
            compile_error("Expected pointer type, cannot dereference expression <%s>", ast_dump_string(operand));
        return ast_new_unary(AST_TYPE_DEREF, operand->ctype->pointer, operand);
    }

    lexer_unget(token);
    return parse_expression_primary();
}

static ast_t *parse_array_decay(ast_t *ast) {
    if (ast->type == AST_TYPE_STRING)
        return ast_new_reference_global(ast_new_pointer(ast_data_char), ast, 0);
    if (ast->ctype->type != TYPE_ARRAY)
        return ast;

    if (ast->type == AST_TYPE_VAR_LOCAL)
        return ast_new_reference_local(ast_new_pointer(ast->ctype->pointer), ast, 0);
    if (ast->type != AST_TYPE_VAR_GLOBAL)
        compile_error("Internal error");

    return ast_new_reference_global(ast_new_pointer(ast->ctype->pointer), ast, 0);
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
        else
            ast = parse_array_decay(ast);

        next = parse_array_decay(parse_expression(pri + !parse_semantic_rightassoc(token->punct)));
        data = parse_semantic_result(token->punct, ast->ctype, next->ctype);

        // swap
        if (!lexer_ispunc(token, '=') && ast->ctype->type != TYPE_PTR && next->ctype->type == TYPE_PTR) {
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
        return ast_data_int;
    if (!strcmp(token->string, "char"))
        return ast_data_char;

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

static ast_t *parse_decl_array_initializer(data_type_t *type) {
    lexer_token_t *token = lexer_next();
    if (type->pointer->type == TYPE_CHAR && token->type == LEXER_TOKEN_STRING)
        return ast_new_string(token->string);

    if (!lexer_ispunc(token, '{'))
        compile_error("Expected initializer list");

    list_t *init = list_create();
    int i;
    for (i = 0; i < type->size; i++) {
        ast_t *in = parse_expression(0);
        list_push(init, in);
        parse_semantic_result('=', in->ctype, type->pointer);
        token = lexer_next();
        if (lexer_ispunc(token, '}') && (i == type->size - 1))
            break;
        if (!lexer_ispunc(token, ','))
            compile_error("Expected comma in initializer list");

        if (i == type->size - 1) {
            token = lexer_next();
            if (!lexer_ispunc(token, '}'))
                compile_error("Expected `}`");
            break;
        }
    }

    return ast_new_array_init(type->size, init);
}

ast_t *parse_decl_initializer(data_type_t *type) {
    if (type->type == TYPE_ARRAY)
        return parse_decl_array_initializer(type);
    return parse_expression(0);
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

    lexer_token_t *varname = token;
    for (;;) {
        token = lexer_next();
        if (lexer_ispunc(token, '[')) {
            ast_t *size = parse_expression(0);
            if (size->type != AST_TYPE_LITERAL || size->ctype->type != TYPE_INT)
                compile_error("Expected integer expression for array size, got <%s>", ast_dump_string(size));
            parse_expect(']');
            type = ast_new_array(type, size->integer);
        } else {
            lexer_unget(token);
            break;
        }
    }

    var = ast_new_variable_local(type, varname->string);
    parse_expect('=');
    ast_t *init = parse_decl_initializer(type);
    parse_expect(';');

    return ast_new_decl(var, init);
}

ast_t *parse_statement_if(void) {
    lexer_token_t *token; //= lexer_next();
    ast_t *cond;
    list_t *then;
    list_t *last;

    parse_expect('(');
    cond = parse_expression(0);
    parse_expect(')');

    parse_expect('{');
    then = parse_block();
    parse_expect('}');

    token = lexer_next();
    if (!token || token->type != LEXER_TOKEN_IDENT || strcmp(token->string, "else")) {
        lexer_unget(token);
        return ast_new_if(cond, then, NULL);
    }

    parse_expect('{');
    last = parse_block();
    parse_expect('}');

    return ast_new_if(cond, then, last);
}

static ast_t *parse_statement(void) {
    lexer_token_t *token = lexer_next();
    ast_t         *ast;

    if (token->type == LEXER_TOKEN_IDENT && !strcmp(token->string, "if"))
        return parse_statement_if();
    lexer_unget(token);

    ast = parse_expression(0);
    parse_expect(';');

    return ast;
}

static ast_t *parse_decl_statement(void) {
    lexer_token_t *token = lexer_peek();

    if (!token)
        return NULL;

    return parse_type_check(token) ? parse_declaration() : parse_statement();
}

list_t *parse_block(void) {
    list_t *statements = list_create();
    for (;;) {
        ast_t *statement = parse_decl_statement();
        if (statement)
            list_push(statements, statement);

        if (!statement || lexer_ispunc(lexer_peek(), '}'))
            break;
    }
    return statements;
}
