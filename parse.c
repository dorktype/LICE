#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lice.h"

#define PARSE_BUFFER 1024
#define PARSE_CALLS  6    // only six registers to use for amd64

////////////////////////////////////////////////////////////////////////
//  prototypes
//      function
static ast_t  *parse_function_call(char *);
//      expression
static ast_t  *parse_expression(int);
static ast_t  *parse_expression_postfix(void);
//      semantic
static void    parse_semantic_lvalue(ast_t *);
static bool    parse_semantic_rightassoc(lexer_token_t *);
//      statement
static ast_t  *parse_statement_declaration_semicolon(void);
//      block
static list_t *parse_block(void);


////////////////////////////////////////////////////////////////////////
// type
//  type_get    -- type for given token
//  type_check  -- check if type exists for given token

static data_type_t *parse_type_get(lexer_token_t *token) {
    if (!token || token->type != LEXER_TOKEN_IDENT)
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

///////////////////////////////////////////////////////////////////////
// misc
//  parse_expect            -- expecting punctuation
//  parse_operator_priority -- get operator priority (precedence)
//  parse_generic           -- dispatch to correct parse routine

static void parse_expect(char punct) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, punct))
        compile_error("Expected `%c`, got %s instead", punct, lexer_tokenstr(token));
}

static int parse_operator_priority(lexer_token_t *token) {
    switch (token->punct) {
        case '=':           return 1;
        case ':':           return 2;
        case '<': case '>': return 3;
        case '+': case '-': return 4;
        case '/': case '*': return 5;
    }
    return -1;
}

static ast_t *parse_generic(char *name) {
    ast_t         *var   = NULL;
    lexer_token_t *token = lexer_next();

    if (lexer_ispunct(token, '('))
        return parse_function_call(name);

    lexer_unget(token);

    if (!(var = ast_find_variable(name)))
        compile_error("Undefined variable: %s", name);

    return var;
}

////////////////////////////////////////////////////////////////////////
// expressions:
//  primary              -- primary expression
//  unary                -- unary expression
//  expression           -- dispatcher
//  expression_semicolon -- dispatcher expecting semicolon
//  expression_subscript -- subscripting expression
//  expression_postfix   -- postfix expression

static ast_t *parse_expression_primary(void) {
    lexer_token_t *token;
    ast_t         *ast;

    if (!(token = lexer_next()))
        return NULL;

    switch (token->type) {
        case LEXER_TOKEN_IDENT:
            return parse_generic(token->string);

        // ast generating ones
        case LEXER_TOKEN_INT:    return ast_new_int(token->integer);
        case LEXER_TOKEN_CHAR:   return ast_new_char(token->character);
        case LEXER_TOKEN_STRING:
            ast = ast_new_string(token->string);
            list_push(ast_globals, ast);
            return ast;

        case LEXER_TOKEN_PUNCT:
            compile_error("Unexpected punctuation: `%c`", token->punct);
            return NULL;
    }
    compile_error("Internal error %s", __func__);
    return NULL;
}

static ast_t *parse_expression_unary(void) {
    lexer_token_t *token = lexer_next();

    if (token->type != LEXER_TOKEN_PUNCT) {
        lexer_unget(token);
        return parse_expression_postfix();
    }

    // for *(expression) and &(expression)
    if (lexer_ispunct(token, '(')) {
        ast_t *next = parse_expression(0);
        parse_expect(')');
        return next;
    }

    if (lexer_ispunct(token, '&')) {
        ast_t *operand = parse_expression_unary();
        parse_semantic_lvalue(operand);
        return ast_new_unary(AST_TYPE_ADDR, ast_new_pointer(operand->ctype), operand);
    }
    if (lexer_ispunct(token, '!')) {
        ast_t *operand = parse_expression_unary();
        return ast_new_unary('!', ast_data_int, operand);
    }

    if (lexer_ispunct(token, '*')) {
        ast_t *operand = parse_expression_unary();
        data_type_t *type = ast_array_convert(operand->ctype);
        if (type->type != TYPE_POINTER)
            compile_error("TODO");
        return ast_new_unary(AST_TYPE_DEREF, operand->ctype->pointer, operand);
    }

    lexer_unget(token);
    return parse_expression_primary();
}

// handles operator precedence climbing as well
static ast_t *parse_expression(int lastpri) {
    ast_t       *ast;
    ast_t       *next;

    // no primary expression?
    if (!(ast = parse_expression_unary()))
        return NULL;

    for (;;) {
        lexer_token_t *token = lexer_next();
        if (token->type != LEXER_TOKEN_PUNCT) {
            lexer_unget(token);
            return ast;
        }

        int pri = parse_operator_priority(token);
        if (pri < 0 || pri < lastpri) {
            lexer_unget(token);
            return ast;
        }

        if (lexer_ispunct(token, '='))
            parse_semantic_lvalue(ast);

        next = parse_expression(pri + !parse_semantic_rightassoc(token));
        ast  = ast_new_binary(token->punct, ast, next);
    }
    return NULL;
}

static ast_t *parse_expression_semicolon(void) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, ';'))
        return NULL;
    lexer_unget(token);

    ast_t *next = parse_expression(0);
    parse_expect(';');
    return next;
}

static ast_t *parse_expression_subscript(ast_t *ast) {
    // generates the following code for subscript
    // (deref (+ ast subscript))
    ast_t *subscript = parse_expression(0);
    parse_expect(']');
    ast_t *node = ast_new_binary('+', ast, subscript);
    return ast_new_unary(AST_TYPE_DEREF, node->ctype->pointer, node);
}

static ast_t *parse_expression_postfix(void) {
    ast_t *node = parse_expression_primary();
    for (;;) {
        lexer_token_t *token = lexer_next();
        if (!token)
            return node;
        if (lexer_ispunct(token, '['))
            node = parse_expression_subscript(node);
        else {
            lexer_unget(token);
            return node;
        }
    }
    return NULL;
}


////////////////////////////////////////////////////////////////////////
// semantics:
//  semantic_lvalue         -- lvalue semantics
//  semantic_rightassoc     -- right associativity semantics

// parser semantic enforcers
static void parse_semantic_lvalue(ast_t *ast) {
    switch (ast->type) {
        case AST_TYPE_VAR_LOCAL:
        case AST_TYPE_VAR_GLOBAL:
        case AST_TYPE_DEREF:
            return;
    }
    compile_error("TODO");
}

static bool parse_semantic_rightassoc(lexer_token_t *token) {
    // enforce right associative semantics
    return (token->punct == '=');
}

////////////////////////////////////////////////////////////////////////
// array
//  array_dimensions_impl    -- array dimensions implementation
//  array_dimensions         -- array dimensions dispatch

static data_type_t *parse_array_dimensions_impl(void) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, '[')) {
        lexer_unget(token);
        return NULL;
    }

    int dimension = -1;
    token = lexer_peek();
    if (!lexer_ispunct(token, ']')) {
        ast_t *size = parse_expression(0);
        if (size->type        != AST_TYPE_LITERAL ||
            size->ctype->type != TYPE_INT) {

            compile_error("TODO");

        }

        dimension = size->integer;
    }

    parse_expect(']');
    // recursive because we're parsing for multi-dimensional
    // arrays here.
    data_type_t *next = parse_array_dimensions_impl();
    if (next) {
        if (next->size == -1 && dimension == -1)
            compile_error("TODO");
        return ast_new_array(next, dimension);
    }
    return ast_new_array(NULL, dimension);
}

static data_type_t *parse_array_dimensions(data_type_t *basetype) {
    data_type_t *type = parse_array_dimensions_impl();
    if (!type)
        return basetype;

    data_type_t *next = type;
    for (; next->pointer; next = next->pointer) {
        // deal with it
        ;
    }
    next->pointer = basetype;

    return type;
}


////////////////////////////////////////////////////////////////////////
// declaration
//  declaration_array_initializer_impl  -- array initializer implementation
//  declaration_array_initializer       -- array initializer dispatch
//  declaration_specification
//  declaration                         -- dispatch

static ast_t *parse_declaration_array_initializer_impl(data_type_t *type) {
    lexer_token_t *token = lexer_next();
    if (type->pointer->type == TYPE_CHAR && token->type == LEXER_TOKEN_STRING)
        return ast_new_string(token->string);

    if (!lexer_ispunct(token, '{'))
        compile_error("Expected initializer list");

    list_t *init = list_create();
    for (;;) {
        token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;
        lexer_unget(token);

        ast_t *in = parse_expression(0);
        list_push(init, in);
        ast_result_type('=', in->ctype, type->pointer);
        token = lexer_next();
        if (!lexer_ispunct(token, ','))
            lexer_unget(token);
    }

    return ast_new_array_init(init);
}

static ast_t *parse_declaration_array_initializer(ast_t *var) {
    ast_t *init;
    if (var->ctype->type == TYPE_ARRAY) {
        init = parse_declaration_array_initializer_impl(var->ctype);
    int len = (init->type == AST_TYPE_STRING)
                    ? strlen(init->string.data) + 1
                    : list_length(init->array);

    if (var->ctype->size == -1) {
        var->ctype->size = len;
    } else if (var->ctype->size != len)
        compile_error("TODO");
    } else {
        init = parse_expression(0);
    }
    parse_expect(';');
    return ast_new_decl(var, init);
}

static data_type_t *parse_declaration_specification(void) {
    lexer_token_t *token = lexer_next();
    data_type_t   *type  = parse_type_get(token);

    if (!type)
        compile_error("Expected type");

    for (;;) {
        token = lexer_next();
        if (!lexer_ispunct(token, '*')) {
            lexer_unget(token);
            return type;
        }

        type = ast_new_pointer(type);
    }
    return NULL;
}

static ast_t *parse_declaration(void) {
    data_type_t   *type = parse_declaration_specification();
    lexer_token_t *name = lexer_next();

    if (name->type != LEXER_TOKEN_IDENT)
        compile_error("Expected identifier");

    // deal with arrays
    type = parse_array_dimensions(type);
    ast_t *var = ast_new_variable_local(type, name->string);
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, '='))
        return parse_declaration_array_initializer(var);
    lexer_unget(token);

    parse_expect(';');
    return ast_new_decl(var, NULL);
}


///////////////////////////////////////////////////////////////////////
// function:
//  function_call
//  function_parameters
//  function_declaration
//  function_list           -- generated list of functions

static ast_t *parse_function_call(char *name) {
    list_t *list = list_create();
    for (;;) {
        // break when call is done
        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, ')'))
            break;
        lexer_unget(token);
        list_push(list, parse_expression(0));
        // deal with call done here as well
        token = lexer_next();
        if (lexer_ispunct(token, ')'))
            break;
        if (!lexer_ispunct(token, ','))
            compile_error("Unexpected character in function call");
    }

    if (PARSE_CALLS < list_length(list))
        compile_error("too many arguments");

    return ast_new_call(ast_data_int, name, list);
}

static list_t *parse_function_parameters(void) {
    list_t        *params = list_create();
    lexer_token_t *token  = lexer_next();

    if (lexer_ispunct(token, ')'))
        return params;

    lexer_unget(token);

    for (;;) {
        data_type_t   *type = parse_declaration_specification();
        lexer_token_t *name = lexer_next();
        if (name->type != LEXER_TOKEN_IDENT)
            compile_error("Expected identifier");

        // proper array decay
        type = parse_array_dimensions(type);
        if (type->type == TYPE_ARRAY)
            type = ast_new_pointer(type->pointer);

        list_push(params, ast_new_variable_local(type, name->string));
        lexer_token_t *next = lexer_next();
        if (lexer_ispunct(next, ')'))
            return params;
        if (!lexer_ispunct(next, ','))
            compile_error("Expected comma");
    }
    return NULL;
}

static ast_t *parse_function_declaration(void) {
    lexer_token_t *token = lexer_peek();
    if (!token)
        return NULL;

    void *ret = parse_declaration_specification();
    lexer_token_t *name = lexer_next();
    if (name->type != LEXER_TOKEN_IDENT)
        compile_error("Expected function name");

    parse_expect('(');
    ast_params = parse_function_parameters();
    parse_expect('{');

    ast_locals = list_create();
    list_t *body = parse_block();
    ast_t  *next = ast_new_function(ret, name->string, ast_params, body, ast_locals);

    ast_locals = NULL;
    ast_params = NULL;

    return next;
}

list_t *parse_function_list(void) {
    list_t *list = list_create();
    for (;;) {
        ast_t *func = parse_function_declaration();
        if (!func)
            return list;
        list_push(list, func);
    }
    return NULL;
}

///////////////////////////////////////////////////////////////////////
// statement
//  statement_try                    -- statement validator
//  statement_return                 -- return statement
//  statement_if                     -- if statement
//  statement_for                    -- for statement
//  statement                        -- generic statement
//  statement_declaration            -- dispatch statement parse or declaration?
//  statement_declaration_semicolon  -- same as above but expecting semicolon

static bool parse_statement_try(lexer_token_t *token, const char *type) {
    return (token->type == LEXER_TOKEN_IDENT && !strcmp(token->string, type));
}

static ast_t *parse_statement_return(void) {
    ast_t *val = parse_expression(0);
    parse_expect(';');
    return ast_new_return(val);
}

ast_t *parse_statement_if(void) {
    lexer_token_t *token;
    ast_t  *cond;
    list_t *then;
    list_t *last;

    parse_expect('(');
    cond = parse_expression(0);
    parse_expect(')');

    parse_expect('{');
    then  = parse_block();
    token = lexer_next();

    if (!token || token->type != LEXER_TOKEN_IDENT || strcmp(token->string, "else")) {
        lexer_unget(token);
        return ast_new_if(cond, then, NULL);
    }

    parse_expect('{');
    last = parse_block();

    return ast_new_if(cond, then, last);
}

static ast_t *parse_statement_for(void) {
    parse_expect('(');
    ast_t *init = parse_statement_declaration_semicolon();
    ast_t *cond = parse_expression_semicolon();
    ast_t *step = lexer_ispunct(lexer_peek(), ')') ? NULL : parse_expression(0);
    parse_expect(')');
    parse_expect('{');

    return ast_new_for(init, cond, step, parse_block());
}

static ast_t *parse_statement(void) {
    lexer_token_t *token = lexer_next();
    ast_t         *ast;

    if (parse_statement_try(token, "if"))     return parse_statement_if();
    if (parse_statement_try(token, "for"))    return parse_statement_for();
    if (parse_statement_try(token, "return")) return parse_statement_return();

    lexer_unget(token);

    ast = parse_expression(0);
    parse_expect(';');

    return ast;
}

static ast_t *parse_statement_declaration(void) {
    lexer_token_t *token = lexer_peek();

    if (!token)
        return NULL;

    return parse_type_check(token) ? parse_declaration() : parse_statement();
}

static ast_t *parse_statement_declaration_semicolon(void) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, ';'))
        return NULL;
    lexer_unget(token);
    return parse_statement_declaration();
}

////////////////////////////////////////////////////////////////////////
// block:
//  block  -- parse block

static list_t *parse_block(void) {
    list_t *statements = list_create();
    for (;;) {
        ast_t *statement = parse_statement_declaration();
        if (statement)
            list_push(statements, statement);

        if (!statement)
            break;

        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;

        lexer_unget(token);
    }
    return statements;
}
