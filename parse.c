#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "lice.h"

#define PARSE_BUFFER    1024
#define PARSE_CALLS     6    // only six registers to use for amd64
#define PARSE_ALIGNMENT 16

static ast_t *parse_expression(void);
static ast_t *parse_statement_compound(void);
static ast_t *parse_statement_declaration(void);
static ast_t *parse_statement(void);
static data_type_t *parse_declaration_intermediate(lexer_token_t **name);

static void parse_semantic_lvalue(ast_t *ast) {
    switch (ast->type) {
        case AST_TYPE_VAR_LOCAL:
        case AST_TYPE_VAR_GLOBAL:
        case AST_TYPE_DEREFERENCE:
        case AST_TYPE_STRUCT:
            return;
    }
    compile_error("Internal error: parse_semantic_lvalue %s", ast_string(ast));
}

static bool parse_semantic_rightassoc(lexer_token_t *token) {
    return (token->punct == '=');
}

static void parse_expect(char punct) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, punct))
        compile_error("Expected `%c`, got %s instead", punct, lexer_tokenstr(token));
}

static bool parse_identifer_check(lexer_token_t *token, const char *identifier) {
    return token->type == LEXER_TOKEN_IDENT && !strcmp(token->string, identifier);
}

static int parse_operator_priority(lexer_token_t *token) {
    switch (token->punct) {
        case '[':
        case '.':
        case LEXER_TOKEN_ARROW:
            return 1;
        case LEXER_TOKEN_INCREMENT:
        case LEXER_TOKEN_DECREMENT:
            return 2;
        case '*':
        case '/':
            return 3;
        case '+':
        case '-':
            return 4;
        case '<':
        case '>':
            return 5;
        case LEXER_TOKEN_EQUAL:
        case LEXER_TOKEN_GEQUAL:
        case LEXER_TOKEN_LEQUAL:
        case LEXER_TOKEN_NEQUAL:
            return 6;
        case '&':
            return 7;
        case '|':
            return 8;
        case LEXER_TOKEN_AND:
            return 9;
        case LEXER_TOKEN_OR:
            return 10;
        case '?':
            return 11;
        case '=':
            return 12;
    }
    return -1;
}

static ast_t *parse_function_call(char *name) {
    list_t *list = list_create();
    for (;;) {
        // break when call is done
        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, ')'))
            break;
        lexer_unget(token);
        list_push(list, parse_expression());
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
            ast_env_push(ast_globalenv, ast);
            return ast;

        case LEXER_TOKEN_PUNCT:
            lexer_unget(token);
            return NULL;

        default:
            break;
    }
    compile_error("Internal error: parse_expression_primary");
    return NULL;
}

static ast_t *parse_expression_subscript(ast_t *ast) {
    // generates the following code for subscript
    // (deref (+ ast subscript))
    ast_t *subscript = parse_expression();
    parse_expect(']');
    ast_t *node = ast_new_binary('+', ast, subscript);
    return ast_new_unary(AST_TYPE_DEREFERENCE, node->ctype->pointer, node);
}

static ast_t *parse_expression_unary(void) {
    lexer_token_t *token = lexer_next();

    if (token->type != LEXER_TOKEN_PUNCT) {
        lexer_unget(token);
        return parse_expression_primary();
    }

    // for *(expression) and &(expression)
    if (lexer_ispunct(token, '(')) {
        ast_t *next = parse_expression();
        parse_expect(')');
        return next;
    }

    if (lexer_ispunct(token, '&')) {
        ast_t *operand = parse_expression_unary();
        parse_semantic_lvalue(operand);
        return ast_new_unary(AST_TYPE_ADDRESS, ast_new_pointer(operand->ctype), operand);
    }
    if (lexer_ispunct(token, '!')) {
        ast_t *operand = parse_expression_unary();
        return ast_new_unary('!', ast_data_int, operand);
    }

    if (lexer_ispunct(token, '*')) {
        ast_t       *operand = parse_expression_unary();
        data_type_t *type    = ast_array_convert(operand->ctype);

        if (type->type != TYPE_POINTER)
            compile_error("Internal error: parse_expression_unary");

        return ast_new_unary(AST_TYPE_DEREFERENCE, operand->ctype->pointer, operand);
    }

    lexer_unget(token);
    return parse_expression_primary();
}

static ast_t *parse_expression_condition(ast_t *condition) {
    ast_t *then = parse_expression();
    parse_expect(':'); // expecting : for ternary
    ast_t *last = parse_expression();
    return ast_new_ternary(then->ctype, condition, then, last);
}

static ast_t *parse_structure_field(ast_t *structure) {
    if (structure->ctype->type != TYPE_STRUCTURE)
        compile_error("Internal error: parse_structure_field (1)");
    lexer_token_t *name = lexer_next();
    if (name->type != LEXER_TOKEN_IDENT)
        compile_error("Internal error: parse_structure_field (2)");

    data_type_t *field = ast_find_structure_field(structure->ctype, name->string);
    return ast_structure_reference_new(structure, field);
}

static ast_t *parse_expression_intermediate(int precision) {
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
        if (pri < 0 || precision <= pri) {
            lexer_unget(token);
            return ast;
        }

        if (lexer_ispunct(token, '?')) {
            ast = parse_expression_condition(ast);
            continue;
        }

        if (lexer_ispunct(token, '.')) {
            ast = parse_structure_field(ast);
            continue;
        }

        // for a->b generate (* (+ a b))
        if (lexer_ispunct(token, LEXER_TOKEN_ARROW)) {
            if (ast->ctype->type != TYPE_POINTER)
                compile_error("Not a valid pointer type: %s", ast_string(ast));
            ast = ast_new_unary(AST_TYPE_DEREFERENCE, ast->ctype->pointer, ast);
            ast = parse_structure_field(ast);
            continue;
        }

        if (lexer_ispunct(token, '[')) {
            ast = parse_expression_subscript(ast);
            continue;
        }

        if (lexer_ispunct(token, LEXER_TOKEN_INCREMENT) ||
            lexer_ispunct(token, LEXER_TOKEN_DECREMENT)) {

            parse_semantic_lvalue(ast);
            ast = ast_new_unary(token->punct, ast->ctype, ast);
            continue;
        }

        if (lexer_ispunct(token, '='))
            parse_semantic_lvalue(ast);

        next = parse_expression_intermediate(pri + !!parse_semantic_rightassoc(token));
        if (!next)
            compile_error("Internal error: parse_expression_intermediate");
        ast = ast_new_binary(token->punct, ast, next);
    }
    return NULL;
}

static ast_t *parse_expression(void) {
    return parse_expression_intermediate(16);
}

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
    return parse_type_get(token) != NULL
                || parse_identifer_check(token, "struct")
                || parse_identifer_check(token, "union");
}

static ast_t *parse_declaration_array_initializer_intermediate(data_type_t *type) {
    lexer_token_t *token = lexer_next();
    if (type->pointer->type == TYPE_CHAR && token->type == LEXER_TOKEN_STRING)
        return ast_new_string(token->string);

    if (!lexer_ispunct(token, '{'))
        compile_error("Expected initializer list");

    list_t *list = list_create();
    for (;;) {
        token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;
        lexer_unget(token);

        ast_t *init = parse_expression();
        list_push(list, init);
        ast_result_type('=', init->ctype, type->pointer);
        token = lexer_next();
        if (!lexer_ispunct(token, ','))
            lexer_unget(token);
    }
    return ast_new_array_init(list);
}

// parses a union/structure tag
static char *parse_memory_tag(void) {
    lexer_token_t *token = lexer_next();
    if (token->type == LEXER_TOKEN_IDENT)
        return token->string;
    lexer_unget(token);
    return NULL;
}

static list_t *parse_memory_fields(void) {
    list_t *list = list_create();
    parse_expect('{');
    for (;;) {
        if (!parse_type_check(lexer_peek()))
            break;

        lexer_token_t *name;
        data_type_t   *type = parse_declaration_intermediate(&name);

        list_push(list, ast_structure_field_new(type, name->string, 0));
        parse_expect(';');
    }
    parse_expect('}');
    return list;
}

static data_type_t *parse_union_definition(void) {
    char        *tag  = parse_memory_tag();
    data_type_t *type = ast_find_memory_definition(ast_unions, tag);

    if (type)
        return type;

    list_t *fields = parse_memory_fields();
    int     size   = 0;

    // calculate the largest size for the union
    for (list_iterator_t *it = list_iterator(fields); !list_iterator_end(it); ) {
        data_type_t *type = list_iterator_next(it);
        size = (size < type->size) ? type->size : size;
    }

    // a 'union' is just a structure with the largest field as the size
    // with many fields, all of which occupy the same 'space'
    data_type_t *fill = ast_structure_new(fields, tag, size);
    list_push(ast_unions, fill);

    return fill;
}

static data_type_t *parse_structure_definition(void) {
    char        *tag  = parse_memory_tag();
    data_type_t *type = ast_find_memory_definition(ast_structures, tag);

    if (type)
        return type;

    list_t *fields = parse_memory_fields();
    int     offset = 0;

    for (list_iterator_t *it = list_iterator(fields); !list_iterator_end(it); ) {
        data_type_t *fieldtype = list_iterator_next(it);
        int          size      = (fieldtype->size < PARSE_ALIGNMENT) ? fieldtype->size : PARSE_ALIGNMENT;

        if (offset % size != 0)
            offset += size - offset % size;

        fieldtype->offset = offset;
        offset += fieldtype->size;
    }

    data_type_t *final = ast_structure_new(fields, tag, offset);
    list_push(ast_structures, final);

    return final;
}

static data_type_t *parse_declaration_specification(void) {
    lexer_token_t *token = lexer_next();

    data_type_t   *type  = parse_identifer_check(token, "struct")
                                ? parse_structure_definition()
                                : (parse_identifer_check(token, "union"))
                                    ? parse_union_definition()
                                    : parse_type_get(token);

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

static void parse_semantic_expression_integer(ast_t *var) {
    if (var->type  != AST_TYPE_LITERAL || var->ctype->type != TYPE_INT)
        compile_error("Internal error: parse_semantic_expression_integer");
}

static ast_t *parse_declaration_initialization_variable(ast_t *var) {
    if (var->ctype->type == TYPE_ARRAY) {
        ast_t *init = parse_declaration_array_initializer_intermediate(var->ctype);
        int len = (init->type == AST_TYPE_STRING)
                        ? strlen(init->string.data) + 1
                        : list_length(init->array);

        if (var->ctype->size == -1) {
            var->ctype->size   = len;
            var->ctype->length = len * var->ctype->pointer->size;
        } else if (var->ctype->length != len) {
            compile_error("Internal error: parse_declaration_array_initializer");
        }
        parse_expect(';');
        return ast_new_decl(var, init);
    }
    ast_t *init = parse_expression();
    parse_expect(';');

    // ensure integer expression
    if (var->type == AST_TYPE_VAR_GLOBAL)
        parse_semantic_expression_integer(init);

    return ast_new_decl(var, init);
}

static data_type_t *parse_array_dimensions_intermediate(data_type_t *basetype) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, '[')) {
        lexer_unget(token);
        return NULL;
    }

    int dimension = -1;
    if (!lexer_ispunct(lexer_peek(), ']')) {
        ast_t *size = parse_expression();
        if (size->type != AST_TYPE_LITERAL || size->ctype->type != TYPE_INT)
            compile_error("Internal error: parse_array_dimensions_impl (1)");
        dimension = size->integer;
    }

    parse_expect(']');
    data_type_t *next = parse_array_dimensions_intermediate(basetype);
    if (next) {
        if (next->length == -1 && dimension == -1)
            compile_error("Internal error: parse_array_dimensions_impl (2)");
        return ast_new_array(next, dimension);
    }
    return ast_new_array(basetype, dimension);
}

static data_type_t *parse_array_dimensions(data_type_t *basetype) {
    data_type_t *data = parse_array_dimensions_intermediate(basetype);
    return (data) ? data : basetype;
}

static ast_t *parse_declaration_initialization(ast_t *var) {
    lexer_token_t *token = lexer_next();

    // global initialization?
    if (lexer_ispunct(token, '='))
        return parse_declaration_initialization_variable(var);

    if (var->ctype->length == -1)
        compile_error("Missing initializer for array");

    lexer_unget(token);
    parse_expect(';');

    return ast_new_decl(var, NULL);
}

static data_type_t *parse_declaration_intermediate(lexer_token_t **name) {
    data_type_t *type = parse_declaration_specification();
    *name = lexer_next();
    if ((*name)->type != LEXER_TOKEN_IDENT)
        compile_error("Internal error: parse_declaration_intermediate %s", ast_type_string(*name));
    return parse_array_dimensions(type);
}

static ast_t *parse_declaration(void) {
    lexer_token_t *token;
    data_type_t   *type = parse_declaration_intermediate(&token);
    return parse_declaration_initialization(ast_new_variable_local(type, token->string));
}

static ast_t *parse_statement_if(void) {
    lexer_token_t *token;
    ast_t  *cond;
    ast_t *then;
    ast_t *last;

    parse_expect('(');
    cond = parse_expression();
    parse_expect(')');


    then  = parse_statement();
    token = lexer_next();

    if (!token || token->type != LEXER_TOKEN_IDENT || strcmp(token->string, "else")) {
        lexer_unget(token);
        return ast_new_if(cond, then, NULL);
    }

    last = parse_statement();
    return ast_new_if(cond, then, last);
}

static ast_t *parse_statement_declaration_semicolon(void) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, ';'))
        return NULL;
    lexer_unget(token);
    return parse_statement_declaration();
}

static ast_t *parse_expression_semicolon(void) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, ';'))
        return NULL;
    lexer_unget(token);
    ast_t *read = parse_expression();
    parse_expect(';');
    return read;
}

static ast_t *parse_statement_for(void) {
    parse_expect('(');
    ast_localenv = ast_env_new(ast_localenv);
    ast_t *init = parse_statement_declaration_semicolon();
    ast_t *cond = parse_expression_semicolon();
    ast_t *step = lexer_ispunct(lexer_peek(), ')') ? NULL : parse_expression();
    parse_expect(')');
    ast_t *body = parse_statement();
    ast_localenv = ast_localenv->next;
    return ast_new_for(init, cond, step, body);
}

static ast_t *parse_statement_return(void) {
    ast_t *val = parse_expression();
    parse_expect(';');
    return ast_new_return(val);
}

static ast_t *parse_statement(void) {
    lexer_token_t *token = lexer_next();
    ast_t         *ast;

    if (parse_identifer_check(token, "if"))     return parse_statement_if();
    if (parse_identifer_check(token, "for"))    return parse_statement_for();
    if (parse_identifer_check(token, "return")) return parse_statement_return();
    if (lexer_ispunct        (token, '{'))      return parse_statement_compound();

    lexer_unget(token);

    ast = parse_expression();
    parse_expect(';');

    return ast;
}

static ast_t *parse_statement_declaration(void) {
    lexer_token_t *token = lexer_peek();

    if (!token)
        return NULL;

    return parse_type_check(token) ? parse_declaration() : parse_statement();
}


static ast_t *parse_statement_compound(void) {
    ast_localenv = ast_env_new(ast_localenv);
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
    ast_localenv = ast_localenv->next;
    return ast_new_compound(statements);
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

static ast_t *parse_function_definition(data_type_t *ret, char *name) {
    list_t *params;
    ast_t  *body;
    ast_t  *next;

    parse_expect('(');
    ast_localenv = ast_env_new(ast_globalenv);
    params       = parse_function_parameters();
    parse_expect('{');

    ast_localenv  = ast_env_new(ast_localenv);
    ast_localvars = list_create();

    body = parse_statement_compound();
    next = ast_new_function(ret, name, params, body, ast_localvars);

    ast_localvars = NULL;

    return next;
}

static ast_t *parse_declaration_function_definition(void) {
    lexer_token_t *token = lexer_peek();
    if (!token)
        return NULL;

    data_type_t   *data = parse_declaration_specification();
    lexer_token_t *name = lexer_next();

    if (name->type != LEXER_TOKEN_IDENT)
        compile_error("TODO: parse_declaration_function_definition");

    data  = parse_array_dimensions(data);
    token = lexer_peek();

    if (lexer_ispunct(token, '=') || data->type == TYPE_ARRAY) {
        ast_t *var = ast_new_variable_global(data, name->string, false);
        return parse_declaration_initialization(var);
    }

    if (lexer_ispunct(token, '('))
        return parse_function_definition(data, name->string);

    if (lexer_ispunct(token, ';')) {
        lexer_next();
        ast_t *var = ast_new_variable_global(data, name->string, false);
        return ast_new_decl(var, NULL);
    }
    compile_error("Internal error: Confused!");
    return NULL;
}

list_t *parse_function_list(void) {
    list_t *list = list_create();
    for (;;) {
        ast_t *func = parse_declaration_function_definition();
        if (!func)
            return list;
        list_push(list, func);
    }
    return NULL;
}
