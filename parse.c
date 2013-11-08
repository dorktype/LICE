#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "lice.h"
#include "lexer.h"

static ast_t       *parse_expression(void);
static ast_t       *parse_expression_unary(void);
static ast_t       *parse_expression_intermediate(int);

static ast_t       *parse_statement_compound(void);
static void         parse_statement_declaration(list_t *);
static ast_t       *parse_statement(void);


static data_type_t *parse_declaration_specification(storage_t *);
static data_type_t *parse_declarator(char **, data_type_t *, list_t *, cdecl_t);
static void         parse_declaration(list_t *, ast_t *(*)(data_type_t *, char *));

static void         parse_function_parameter(data_type_t **, char **, bool);
static data_type_t *parse_function_parameters(list_t *, data_type_t *);

table_t *parse_typedefs = &SENTINEL_TABLE;

static bool parse_type_check(lexer_token_t *token);

static void parse_semantic_lvalue(ast_t *ast) {
    switch (ast->type) {
        case AST_TYPE_VAR_LOCAL:
        case AST_TYPE_VAR_GLOBAL:
        case AST_TYPE_DEREFERENCE:
        case AST_TYPE_STRUCT:
            return;
    }
    compile_error("expected lvalue, `%s' isn't a valid lvalue", ast_string(ast));
}

static void parse_semantic_notvoid(data_type_t *type) {
    if (type->type == TYPE_VOID)
        compile_error("void not allowed in expression");
}

static void parse_semantic_integer(ast_t *node) {
    if (!ast_type_integer(node->ctype))
        compile_error("expected integer type, `%s' isn't a valid integer type", ast_string(node));
}

static bool parse_semantic_rightassoc(lexer_token_t *token) {
    return (token->punct == '=');
}

static void parse_expect(char punct) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, punct))
        compile_error("expected `%c`, got %s instead", punct, lexer_tokenstr(token));
}

static bool parse_identifer_check(lexer_token_t *token, const char *identifier) {
    return token->type == LEXER_TOKEN_IDENTIFIER && !strcmp(token->string, identifier);
}

static int parse_evaluate(ast_t *ast) {
    switch (ast->type) {
        case AST_TYPE_LITERAL:
            if (ast_type_integer(ast->ctype))
                return ast->integer;
            compile_error("not a valid integer constant expression `%s'", ast_string(ast));
            break;

        case '+':                return parse_evaluate(ast->left) +  parse_evaluate(ast->right);
        case '-':                return parse_evaluate(ast->left) -  parse_evaluate(ast->right);
        case '*':                return parse_evaluate(ast->left) *  parse_evaluate(ast->right);
        case '/':                return parse_evaluate(ast->left) /  parse_evaluate(ast->right);
        case '<':                return parse_evaluate(ast->left) <  parse_evaluate(ast->right);
        case '>':                return parse_evaluate(ast->left) >  parse_evaluate(ast->right);
        case '^':                return parse_evaluate(ast->left) ^  parse_evaluate(ast->right);
        case '%':                return parse_evaluate(ast->left) %  parse_evaluate(ast->right);
        case LEXER_TOKEN_AND:    return parse_evaluate(ast->left) && parse_evaluate(ast->right);
        case LEXER_TOKEN_OR:     return parse_evaluate(ast->left) || parse_evaluate(ast->right);
        case LEXER_TOKEN_EQUAL:  return parse_evaluate(ast->left) == parse_evaluate(ast->right);
        case LEXER_TOKEN_LEQUAL: return parse_evaluate(ast->left) <= parse_evaluate(ast->right);
        case LEXER_TOKEN_GEQUAL: return parse_evaluate(ast->left) >= parse_evaluate(ast->right);
        case LEXER_TOKEN_NEQUAL: return parse_evaluate(ast->left) != parse_evaluate(ast->right);
        case LEXER_TOKEN_LSHIFT: return parse_evaluate(ast->left) << parse_evaluate(ast->right);
        case LEXER_TOKEN_RSHIFT: return parse_evaluate(ast->left) >> parse_evaluate(ast->right);

        /* Deal with unary operations differently */
        case '!':                      return !parse_evaluate(ast->unary.operand);
        case '~':                      return ~parse_evaluate(ast->unary.operand);
        case AST_TYPE_EXPRESSION_CAST: return  parse_evaluate(ast->unary.operand);

        default:
            compile_error("not a valid integer constant expression `%s'", ast_string(ast));
    }
    return -1;
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
        case '%':
            return 3;
        case '+':
        case '-':
            return 4;
        case LEXER_TOKEN_LSHIFT:
        case LEXER_TOKEN_RSHIFT:
            return 5;
        case '<':
        case '>':
            return 6;
        case LEXER_TOKEN_EQUAL:
        case LEXER_TOKEN_GEQUAL:
        case LEXER_TOKEN_LEQUAL:
        case LEXER_TOKEN_NEQUAL:
            return 7;
        case '&':
            return 8;
        case '^':
            return 9;
        case '|':
            return 10;
        case LEXER_TOKEN_AND:
            return 11;
        case LEXER_TOKEN_OR:
            return 12;
        case '?':
            return 13;
        case '=':
        case LEXER_TOKEN_COMPOUND_ADD:
        case LEXER_TOKEN_COMPOUND_AND:
        case LEXER_TOKEN_COMPOUND_DIV:
        case LEXER_TOKEN_COMPOUND_LSHIFT:
        case LEXER_TOKEN_COMPOUND_MOD:
        case LEXER_TOKEN_COMPOUND_MUL:
        case LEXER_TOKEN_COMPOUND_OR:
        case LEXER_TOKEN_COMPOUND_RSHIFT:
        case LEXER_TOKEN_COMPOUND_SUB:
        case LEXER_TOKEN_COMPOUND_XOR:
            return 14;
    }
    return -1;
}

static list_t *parse_parameter_types(list_t *parameters) {
    list_t *list = list_create();
    for (list_iterator_t *it = list_iterator(parameters); !list_iterator_end(it); )
        list_push(list, ((ast_t*)list_iterator_next(it))->ctype);
    return list;
}

static void parse_function_typecheck(const char *name, list_t *parameters, list_t *arguments) {
    if (list_length(arguments) < list_length(parameters))
        compile_error("too few arguments for function `%s'", name);
    for (list_iterator_t *it = list_iterator(parameters),
                         *jt = list_iterator(arguments); !list_iterator_end(jt); )
    {
        data_type_t *parameter = list_iterator_next(it);
        data_type_t *argument  = list_iterator_next(jt);

        if (parameter)
            ast_result_type('=', parameter, argument);
        else
            ast_result_type('=', argument, ast_data_table[AST_DATA_INT]);
    }
}

static ast_t *parse_function_call(char *name) {
    list_t *list = list_create();
    for (;;) {

        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, ')'))
            break;
        lexer_unget(token);
        list_push(list, parse_expression());

        token = lexer_next();
        if (lexer_ispunct(token, ')'))
            break;
        if (!lexer_ispunct(token, ','))
            compile_error("unexpected token `%s'", lexer_tokenstr(token));
    }

    if (ARCH_CALLREGISTERS < list_length(list))
        compile_error("too many arguments");

    ast_t *func = table_find(ast_localenv, name);
    if (func) {
        data_type_t *declaration = func->ctype;
        if (declaration->type != TYPE_FUNCTION)
            compile_error("expected a function name, `%s' isn't a function", name);
        parse_function_typecheck(name, declaration->parameters, parse_parameter_types(list));
        return ast_call(declaration->returntype, name, list, declaration->parameters);
    }
    /* TODO: warn about implicit int return */
    return ast_call(ast_data_table[AST_DATA_INT], name, list, list_create());
}


static ast_t *parse_generic(char *name) {
    ast_t         *var   = NULL;
    lexer_token_t *token = lexer_next();

    if (lexer_ispunct(token, '('))
        return parse_function_call(name);

    lexer_unget(token);

    if (!(var = table_find(ast_localenv, name)))
        compile_error("undefined variable `%s'", name);

    return var;
}

static ast_t *parse_number_integer(const char *string) {
    const char *p    = string;
    int   base = 10;

    if (!strncasecmp(string, "0x", 2)) {
        base = 16;
        p++;
        p++;
    } else if (string[0] == '0' && string[1] != '\0') {
        base = 8;
        p++;
    }

    while (isxdigit(*p)) {
        if (base == 10 && isalpha(*p))
            compile_error("invalid character in decimal literal");
        if (base == 8 && !('0' <= *p && *p <= '7'))
            compile_error("invalid character in octal literal");
        p++;
    }

    if (!strcasecmp(p, "l"))
        return ast_new_integer(ast_data_table[AST_DATA_LONG], strtol(string, NULL, base));
    if (!strcasecmp(p, "ul") || !strcasecmp(p, "lu"))
        return ast_new_integer(ast_data_table[AST_DATA_ULONG], strtoul(string, NULL, base));
    if (!strcasecmp(p, "ll"))
        return ast_new_integer(ast_data_table[AST_DATA_LLONG], strtoll(string, NULL, base));
    if (!strcasecmp(p, "ull") || !strcasecmp(p, "llu"))
        return ast_new_integer(ast_data_table[AST_DATA_ULLONG], strtoull(string, NULL, base));
    if (*p != '\0')
        compile_error("invalid suffix for literal");

    long value = strtol(string, NULL, base);
    return (value & ~(long)UINT_MAX)
                ? ast_new_integer(ast_data_table[AST_DATA_LONG], value)
                : ast_new_integer(ast_data_table[AST_DATA_INT], value);
}

static ast_t *parse_number_floating(const char *string) {
    const char *p = string;
    char *end;

    while (p[1])
        p++;

    ast_t *ast;
    if (*p == 'l' || *p == 'L')
        ast = ast_new_floating(ast_data_table[AST_DATA_LDOUBLE], strtold(string, &end));
    else if (*p == 'f' || *p == 'F')
        ast = ast_new_floating(ast_data_table[AST_DATA_FLOAT], strtof(string, &end));
    else {
        ast = ast_new_floating(ast_data_table[AST_DATA_DOUBLE], strtod(string, &end));
        p++;
    }

    if (end != p)
        compile_error("malformatted float literal");

    return ast;
}

static ast_t *parse_number(const char *string) {
    return strpbrk(string, ".pe")
                ? parse_number_floating(string)
                : parse_number_integer(string);
}

static ast_t *parse_expression_primary(void) {
    lexer_token_t *token;
    ast_t         *ast;

    if (!(token = lexer_next()))
        return NULL;

    switch (token->type) {
        case LEXER_TOKEN_IDENTIFIER:
            return parse_generic(token->string);
        case LEXER_TOKEN_NUMBER:
            return parse_number(token->string);
        case LEXER_TOKEN_CHAR:
            return ast_new_integer(ast_data_table[AST_DATA_CHAR], token->character);
        case LEXER_TOKEN_STRING:
            ast = ast_new_string(token->string);
            list_push(ast_strings, ast);
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
    ast_t *subscript = parse_expression();
    parse_expect(']');
    ast_t *node = ast_new_binary('+', ast, subscript);
    return ast_new_unary(AST_TYPE_DEREFERENCE, node->ctype->pointer, node);
}

static ast_t *parse_sizeof(bool typename) {
    lexer_token_t *token = lexer_next();
    if (typename && parse_type_check(token)) {
        lexer_unget(token);
        data_type_t *type;
        parse_function_parameter(&type, NULL, true);
        return ast_new_integer(ast_data_table[AST_DATA_LONG], type->size);
    }

    if (lexer_ispunct(token, '(')) {
        ast_t *next = parse_sizeof(true);
        parse_expect(')');
        return next;
    }

    lexer_unget(token);
    ast_t *expression = parse_expression_unary();
    if (expression->ctype->size == 0)
        compile_error("sizeof(void) illegal");
    return ast_new_integer(ast_data_table[AST_DATA_LONG], expression->ctype->size);
}

static ast_t *parse_expression_unary_cast(void) {
    data_type_t *basetype = parse_declaration_specification(NULL);
    data_type_t *casttype = parse_declarator(NULL, basetype, NULL, CDECL_CAST);

    parse_expect(')');
    ast_t *expression = parse_expression_unary();

    return ast_new_unary(AST_TYPE_EXPRESSION_CAST, casttype, expression);
}

static ast_t *parse_expression_unary(void) {
    lexer_token_t *token = lexer_next();

    if (!token)
        compile_error("unexpected end of input");

    if (parse_identifer_check(token, "sizeof")) {
        return parse_sizeof(false);
    }

    if (lexer_ispunct(token, '(')) {
        if (parse_type_check(lexer_peek()))
            return parse_expression_unary_cast();
        ast_t *next = parse_expression();
        parse_expect(')');
        return next;
    }
    if (lexer_ispunct(token, '&')) {
        ast_t *operand = parse_expression_intermediate(3);
        parse_semantic_lvalue(operand);
        return ast_new_unary(AST_TYPE_ADDRESS, ast_pointer(operand->ctype), operand);
    }
    if (lexer_ispunct(token, '!')) {
        ast_t *operand = parse_expression_intermediate(3);
        return ast_new_unary('!', ast_data_table[AST_DATA_INT], operand);
    }
    if (lexer_ispunct(token, '-')) {
        ast_t *ast = parse_expression_intermediate(3);
        return ast_new_binary('-', ast_new_integer(ast_data_table[AST_DATA_INT], 0), ast);
    }
    if (lexer_ispunct(token, '~')) {
        ast_t *ast = parse_expression_intermediate(3);
        if (!ast_type_integer(ast->ctype))
            compile_error("invalid expression `%s'", ast_string(ast));
        return ast_new_unary('~', ast->ctype, ast);
    }
    if (lexer_ispunct(token, '*')) {
        ast_t       *operand = parse_expression_intermediate(3);
        data_type_t *type    = ast_array_convert(operand->ctype);

        if (type->type != TYPE_POINTER)
            compile_error("expected pointer type, `%s' isn't pointer type", ast_string(operand));

        return ast_new_unary(AST_TYPE_DEREFERENCE, operand->ctype->pointer, operand);
    }
    if (lexer_ispunct(token, LEXER_TOKEN_INCREMENT)
    ||  lexer_ispunct(token, LEXER_TOKEN_DECREMENT))
    {
        ast_t *next = parse_expression_intermediate(3);
        parse_semantic_lvalue(next);
        int operand = lexer_ispunct(token, LEXER_TOKEN_INCREMENT)
                            ? AST_TYPE_PRE_INCREMENT
                            : AST_TYPE_PRE_DECREMENT;
        return ast_new_unary(operand, next->ctype, next);
    }

    lexer_unget(token);
    return parse_expression_primary();
}

static ast_t *parse_expression_condition(ast_t *condition) {
    ast_t *then = parse_expression();
    parse_expect(':');
    ast_t *last = parse_expression();
    return ast_ternary(then->ctype, condition, then, last);
}

static ast_t *parse_structure_field(ast_t *structure) {
    if (structure->ctype->type != TYPE_STRUCTURE)
        compile_error("expected structure type, `%s' isn't structure type", ast_string(structure));
    lexer_token_t *name = lexer_next();
    if (name->type != LEXER_TOKEN_IDENTIFIER)
        compile_error("expected field name, got `%s' instead", lexer_tokenstr(name));

    data_type_t *field = table_find(structure->ctype->fields, name->string);
    if (!field)
        compile_error("structure has no such field `%s'", lexer_tokenstr(name));
    return ast_structure_reference(field, structure, name->string);
}

static int parse_operation_compound_operator(lexer_token_t *token) {
    if (token->type != LEXER_TOKEN_PUNCT)
        return 0;

    switch (token->punct) {
        case LEXER_TOKEN_COMPOUND_RSHIFT: return LEXER_TOKEN_RSHIFT;
        case LEXER_TOKEN_COMPOUND_LSHIFT: return LEXER_TOKEN_LSHIFT;
        case LEXER_TOKEN_COMPOUND_ADD:    return '+';
        case LEXER_TOKEN_COMPOUND_AND:    return '&';
        case LEXER_TOKEN_COMPOUND_DIV:    return '/';
        case LEXER_TOKEN_COMPOUND_MOD:    return '%';
        case LEXER_TOKEN_COMPOUND_MUL:    return '*';
        case LEXER_TOKEN_COMPOUND_OR:     return '|';
        case LEXER_TOKEN_COMPOUND_SUB:    return '-';
        case LEXER_TOKEN_COMPOUND_XOR:    return '^';
        default:
            return 0;
    }

    return -1;
}

static int parse_operation_reclassify(int punct) {
    switch (punct) {
        case LEXER_TOKEN_LSHIFT: return AST_TYPE_LSHIFT;
        case LEXER_TOKEN_RSHIFT: return AST_TYPE_RSHIFT;
        case LEXER_TOKEN_EQUAL:  return AST_TYPE_EQUAL;
        case LEXER_TOKEN_GEQUAL: return AST_TYPE_GEQUAL;
        case LEXER_TOKEN_LEQUAL: return AST_TYPE_LEQUAL;
        case LEXER_TOKEN_NEQUAL: return AST_TYPE_NEQUAL;
        case LEXER_TOKEN_AND:    return AST_TYPE_AND;
        case LEXER_TOKEN_OR:     return AST_TYPE_OR;
        default:
            break;
    }
    return punct;
}

static bool parse_operation_integer_check(int operation) {
    return operation == '^'
        || operation == '%'
        || operation == LEXER_TOKEN_LSHIFT
        || operation == LEXER_TOKEN_RSHIFT;
}

static ast_t *parse_expression_intermediate(int precision) {
    ast_t       *ast;
    ast_t       *next;

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
            int operand = lexer_ispunct(token, LEXER_TOKEN_INCREMENT)
                                ? AST_TYPE_POST_INCREMENT
                                : AST_TYPE_POST_DECREMENT;

            ast = ast_new_unary(operand, ast->ctype, ast);
            continue;
        }

        int compound = parse_operation_compound_operator(token);
        if (lexer_ispunct(token, '=') || compound)
            parse_semantic_lvalue(ast);

        next = parse_expression_intermediate(pri + !!parse_semantic_rightassoc(token));
        if (!next)
            compile_error("Internal error: parse_expression_intermediate (next)");
        int operation = compound ? compound : token->punct;
        int op        = parse_operation_reclassify(operation);

        if (parse_operation_integer_check(op)) {
            parse_semantic_integer(ast);
            parse_semantic_integer(next);
        }

        if (compound)
            ast = ast_new_binary('=', ast, ast_new_binary(op, ast, next));
        else
            ast = ast_new_binary(op, ast, next);
    }
    return NULL;
}

static ast_t *parse_expression(void) {
    return parse_expression_intermediate(16);
}

static bool parse_type_check(lexer_token_t *token) {
    if (token->type != LEXER_TOKEN_IDENTIFIER)
        return false;

    static const char *keywords[] = {
        "char",     "short",  "int",     "long",     "float",    "double",
        "struct",   "union",  "signed",  "unsigned", "enum",     "void",
        "typedef",  "extern", "static",  "auto",     "register", "const",
        "volatile", "inline", "restrict"
    };

    for (int i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
        if (!strcmp(keywords[i], token->string))
            return true;

    if (table_find(parse_typedefs, token->string))
        return true;

    return false;
}

static void parse_declaration_initializerlist_element(list_t *list, data_type_t *type) {
    lexer_token_t *token = lexer_peek();
    ast_t         *init  = parse_expression();

    if (!init)
        compile_error("expected expression, `%s' isn't an expression", ast_string(init));

    ast_result_type('=', init->ctype, type);

    init->initlist.type = type;
    token = lexer_next();

    if (!lexer_ispunct(token, ','))
        lexer_unget(token);

    list_push(list, init);
}

static void parse_declaration_array_initializer_intermediate(list_t *initlist, data_type_t *type) {
    lexer_token_t *token = lexer_next();

    if (type->pointer->type == TYPE_CHAR && token->type == LEXER_TOKEN_STRING) {
        for (char *p = token->string; *p; p++) {
            ast_t *value = ast_new_integer(ast_data_table[AST_DATA_CHAR], *p);
            value->initlist.type = ast_data_table[AST_DATA_CHAR];
            list_push(initlist, value);
        }
        ast_t *value = ast_new_integer(ast_data_table[AST_DATA_CHAR], '\0');
        value->initlist.type = ast_data_table[AST_DATA_CHAR];
        list_push(initlist, value);
        return;
    }

    if (!lexer_ispunct(token, '{'))
        compile_error("expected initializer list");

    for (;;) {
        token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;
        lexer_unget(token);

        parse_declaration_initializerlist_element(initlist, type->pointer);
    }
}

static ast_t *parse_declaration_array_initializer_value(data_type_t *type) {
    list_t *initlist = list_create();
    parse_declaration_array_initializer_intermediate(initlist, type);
    ast_t *init = ast_initializerlist(initlist);

    int length = (init->type == AST_TYPE_STRING)
                    ? strlen(init->string.data)
                    : list_length(init->initlist.list);

    if (type->length == -1) {
        type->length = length;
        type->size   = length * type->pointer->size;
    } else if (type->length != length) {
        compile_error("Internal error %s", __func__);
    }
    return init;
}

static ast_t *parse_declaration_structure_initializer_value(data_type_t *type) {
    parse_expect('{');
    list_t *initlist = list_create();
    for (list_iterator_t *it = list_iterator(table_values(type->fields)); !list_iterator_end(it); ) {
        data_type_t   *fieldtype = list_iterator_next(it);
        lexer_token_t *token     = lexer_next();

        if (lexer_ispunct(token, '}'))
            return ast_initializerlist(initlist);

        if (lexer_ispunct(token, '{')) {
            if (fieldtype->type != TYPE_ARRAY)
                compile_error("Internal error %s", __func__);
            lexer_unget(token);
            parse_declaration_array_initializer_intermediate(initlist, fieldtype);
            continue;
        }

        lexer_unget(token);
        parse_declaration_initializerlist_element(initlist, fieldtype);
    }
    parse_expect('}');
    return ast_initializerlist(initlist);
}

static char *parse_memory_tag(void) {
    lexer_token_t *token = lexer_next();
    if (token->type == LEXER_TOKEN_IDENTIFIER)
        return token->string;
    lexer_unget(token);
    return NULL;
}

static int parse_memory_fields_padding(int offset, int size) {
    size = MIN(size, ARCH_ALIGNMENT);
    return (offset % size == 0) ? 0 : size - offset % size;
}

static void parse_memory_fields_squash(table_t *table, data_type_t *unnamed, int offset) {
    for (list_iterator_t *it = list_iterator(table_keys(unnamed->fields)); !list_iterator_end(it); ) {
        char         *name = list_iterator_next(it);
        data_type_t  *type = ast_type_copy(table_find(unnamed->fields, name));
        type->offset += offset;
        table_insert(table, name, type);
    }
}

static table_t *parse_memory_fields(int *rsize, bool isstruct) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, '{')) {
        lexer_unget(token);
        return NULL;
    }

    int      offset  = 0;
    int      maxsize = 0;
    table_t *table   = table_create(NULL);

    for (;;) {
        if (!parse_type_check(lexer_peek()))
            break;

        data_type_t *basetype = parse_declaration_specification(NULL);

        if (basetype->type == TYPE_STRUCTURE && lexer_ispunct(lexer_peek(), ';')) {
            lexer_next(); /* Skip */
            parse_memory_fields_squash(table, basetype, offset);
            if (isstruct)
                offset += basetype->size;
            else
                maxsize = MAX(maxsize, basetype->size);
            continue;
        }

        for (;;) {
            char        *name;
            data_type_t *fieldtype = parse_declarator(&name, basetype, NULL, CDECL_PARAMETER);

            parse_semantic_notvoid(fieldtype);

            if (isstruct) {
                offset   += parse_memory_fields_padding(offset, fieldtype->size);
                fieldtype = ast_structure_field(fieldtype, offset);
                offset   += fieldtype->size;
            } else {
                maxsize   = MAX(maxsize, fieldtype->size);
                fieldtype = ast_structure_field(fieldtype, 0);
            }
            table_insert(table, name, fieldtype);

            token = lexer_next();
            if (lexer_ispunct(token, ','))
                continue;

            lexer_unget(token);
            parse_expect(';');
            break;
        }
    }
    parse_expect('}');
    *rsize = isstruct ? offset : maxsize;
    return table;
}

static data_type_t *parse_tag_definition(table_t *table, bool isstruct) {
    char        *tag    = parse_memory_tag();
    int          size   = 0;
    table_t     *fields = parse_memory_fields(&size, isstruct);
    data_type_t *r;

    if (tag) {
        if (!(r = table_find(table, tag))) {
            r = ast_structure_new(NULL, 0);
            table_insert(table, tag, r);
        }
    } else {
        r = ast_structure_new(NULL, 0);
        if (tag)
            table_insert(table, tag, r);
    }

    if (r && !fields)
        return r;

    if (r && fields) {
        r->fields = fields;
        r->size   = size;
        return r;
    }

    return r;
}

static data_type_t *parse_enumeration(void) {
    lexer_token_t *token = lexer_next();
    if (token->type == LEXER_TOKEN_IDENTIFIER)
        token = lexer_next();
    if (!lexer_ispunct(token, '{')) {
        lexer_unget(token);
        return ast_data_table[AST_DATA_INT];
    }
    int accumulate = 0;
    for (;;) {
        token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;

        if (token->type != LEXER_TOKEN_IDENTIFIER)
            compile_error("NOPE");

        char *name = token->string;
        token = lexer_next();
        if (lexer_ispunct(token, '='))
            accumulate = parse_evaluate(parse_expression());
        else
            lexer_unget(token);

        ast_t *constval = ast_new_integer(ast_data_table[AST_DATA_INT], accumulate++);
        table_insert(ast_localenv ? ast_localenv : ast_globalenv, name, constval);
        token = lexer_next();
        if (lexer_ispunct(token, ','))
            continue;
        if (lexer_ispunct(token, '}'))
            break;

        compile_error("NOPE!");
    }
    return ast_data_table[AST_DATA_INT];
}

static data_type_t *parse_declaration_specification(storage_t *rstorage) {
    storage_t      storage = 0;
    lexer_token_t *token   = lexer_peek();
    if (!token || token->type != LEXER_TOKEN_IDENTIFIER)
        compile_error("internal error in declaration specification parsing");

    /*
     *  large declaration specification state machine:
     *    There is six pieces of state to the following state machine
     *    for dealing with all the permutations of declaration
     *    specification.
     *
     *    1: The type, most common of course, this is the "base type"
     *       of the declaration.
     *
     *    2: The size, in C, types are also size specifiers on types,
     *       e.g short int, long int, long long int, act as 'sizes'.
     *       Short and long are not technically types, people who use
     *       them as is without a type associated with them (like unsigned)
     *       concludes implicit int. There is no situation where a size
     *       specifier would couple with anything but an int type. It
     *       should be noted that there has to be an "unsized" state for
     *       types on their own.
     *
     *    3: The Signness/signature, for knowing if the declaration is
     *       signed or unsigned. This isn't actually a boolean state because
     *       there needs to be an unsignness state since the char type is
     *       allowed to have it's signeness implementation-defined.
     *
     *    4: constantness
     *         self explanatory
     *    5: vollatileness
     *         self explanatory
     *    6: inlineness
     *         self explanatory
     *
     *    7: user (can include redundant partial specification), e.g
     *        typedef unsigned int foo; signed foo; <--- what to do?
     *        this also includes enums, unions, and structures.
     */
    enum {
        kvoid = 1,
        kchar,
        kint,
        kfloat,
        kdouble
    } type = 0;

    enum {
        kunsize,
        kshort,
        klong,
        kllong
    } size = kunsize;

    enum {
        ksigned = 1,
        kunsigned
    } signature = 0;

    bool __attribute__((unused)) kconst    = false;
    bool __attribute__((unused)) kvolatile = false;
    bool __attribute__((unused)) kinline   = false;

    data_type_t *user = NULL;
    data_type_t *find = NULL;

    #define set_uncheck(STATE, VALUE)                                  \
        do {                                                           \
            STATE = VALUE;                                             \
        } while (0)

    #define set_check(STATE, VALUE)                                    \
        do {                                                           \
            if (STATE != 0) {                                          \
                goto state_machine_error;                              \
            }                                                          \
            set_uncheck(STATE, VALUE);                                 \
        } while (0)

    #define set_state(STATE, VALUE)                                    \
        do {                                                           \
            set_check(STATE, VALUE);                                   \
            switch (size) {                                            \
                case kshort:                                           \
                    if (type != 0 && type != kint)                     \
                        goto state_machine_error;                      \
                    break;                                             \
                case klong:                                            \
                    if (type != 0 && type != kint && type != kdouble)  \
                        goto state_machine_error;                      \
                    break;                                             \
                default:                                               \
                    break;                                             \
            }                                                          \
            if (signature != 0) {                                      \
                switch (type) {                                        \
                    case kvoid:                                        \
                    case kfloat:                                       \
                    case kdouble:                                      \
                        goto state_machine_error;                      \
                        break;                                         \
                    default:                                           \
                        break;                                         \
                }                                                      \
            }                                                          \
            if (user && (type != 0 || size != 0 || signature != 0))    \
                goto state_machine_error;                              \
        } while (0)

    #define set_class(VALUE)                                           \
        do {                                                           \
            if (VALUE == 0)                                            \
                goto state_machine_error;                              \
            set_check(storage, VALUE);                                 \
        } while (0)

    #define state_machine_try(THING) \
        if (!strcmp(token->string, THING))

    for (;;) {
        token = lexer_next();
        if (!token)
            compile_error("type specification with unexpected ending");

        if (token->type != LEXER_TOKEN_IDENTIFIER) {
            lexer_unget(token);
            break;
        }

             state_machine_try("const")    kconst    = true;
        else state_machine_try("volatile") kvolatile = true;
        else state_machine_try("inline")   kinline   = true;

        else state_machine_try("typedef")  set_class(STORAGE_TYPEDEF);
        else state_machine_try("extern")   set_class(STORAGE_EXTERN);
        else state_machine_try("static")   set_class(STORAGE_STATIC);
        else state_machine_try("auto")     set_class(STORAGE_AUTO);
        else state_machine_try("register") set_class(STORAGE_REGISTER);

        else state_machine_try("void")     set_state(type,      kvoid);
        else state_machine_try("char")     set_state(type,      kchar);
        else state_machine_try("int")      set_state(type,      kint);
        else state_machine_try("float")    set_state(type,      kfloat);
        else state_machine_try("double")   set_state(type,      kdouble);

        else state_machine_try("signed")   set_state(signature, ksigned);
        else state_machine_try("unsigned") set_state(signature, kunsigned);
        else state_machine_try("short")    set_state(size,      kshort);

        else state_machine_try("struct")   set_state(user,      parse_tag_definition(ast_structures, true));
        else state_machine_try("union")    set_state(user,      parse_tag_definition(ast_unions,     false));
        else state_machine_try("enum")     set_state(user,      parse_enumeration());
        else state_machine_try("long") {
            switch (size) {
                case kunsize:
                    set_state(size, klong);
                    break;
                case klong:
                    set_uncheck(size, kllong);
                    break;
                default:
                    goto state_machine_error;
            }
        } else if ((find = table_find(parse_typedefs, token->string))) {
            set_state(user, find);
        } else {
            lexer_unget(token);
            break;
        }

        #undef set_check
        #undef set_class
        #undef set_state
        #undef set_uncheck
    }

    if (rstorage)
        *rstorage = storage;

    if (user)
        return user;

    switch (type) {
        case kvoid:
            return ast_data_table[AST_DATA_VOID];
        case kchar:
            return ast_type_create(TYPE_CHAR,  signature != kunsigned);
        case kfloat:
            return ast_type_create(TYPE_FLOAT, false);
        case kdouble:
            return ast_type_create(
                (size == klong)
                    ? TYPE_LDOUBLE
                    : TYPE_DOUBLE,
                false
            );
        default:
            break;
    }

    switch (size) {
        case kshort:
            return ast_type_create(TYPE_SHORT, signature != kunsigned);
        case klong:
            return ast_type_create(TYPE_LONG,  signature != kunsigned);
        case kllong:
            return ast_type_create(TYPE_LLONG, signature != kunsigned);
        default:
            /*
             * You also need to deal with implicit int given the right
             * conditions of the state machine.
             */
            return ast_type_create(TYPE_INT, signature != kunsigned);
    }

    compile_error("ICE (BAD)");
state_machine_error:
    compile_error("ICE (GOOD)");

    return NULL;
}

static ast_t *parse_declaration_initialization_value(data_type_t *type) {
    return (type->type == TYPE_ARRAY)
                ? parse_declaration_array_initializer_value(type)
                : (type->type == TYPE_STRUCTURE)
                        ? parse_declaration_structure_initializer_value(type)
                        : parse_expression();
}

static data_type_t *parse_array_dimensions_intermediate(data_type_t *basetype) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, '[')) {
        lexer_unget(token);
        return NULL;
    }

    int dimension = -1;
    if (!lexer_ispunct(lexer_peek(), ']'))
        dimension = parse_evaluate(parse_expression());

    parse_expect(']');
    data_type_t *next = parse_array_dimensions_intermediate(basetype);
    if (next) {
        if (next->length == -1 && dimension == -1)
            compile_error("Internal error: parse_array_dimensions_intermediate (2)");
        return ast_array(next, dimension);
    }
    return ast_array(basetype, dimension);
}

static data_type_t *parse_array_dimensions(data_type_t *basetype) {
    data_type_t *data = parse_array_dimensions_intermediate(basetype);
    return (data) ? data : basetype;
}

static ast_t *parse_declaration_initialization(ast_t *var) {
    ast_t *init = parse_declaration_initialization_value(var->ctype);
    if (var->type == AST_TYPE_VAR_GLOBAL && ast_type_integer(var->ctype))
        init = ast_new_integer(ast_data_table[AST_DATA_INT], parse_evaluate(init));
    return ast_declaration(var, init);
}

static void parse_function_parameter(data_type_t **rtype, char **name, bool next) {
    data_type_t *basetype;
    storage_t    storage;

    basetype = parse_declaration_specification(&storage);
    basetype = parse_declarator(name, basetype, NULL, next ? CDECL_TYPEONLY : CDECL_PARAMETER);
    *rtype = parse_array_dimensions(basetype);
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

    if (!token || token->type != LEXER_TOKEN_IDENTIFIER || strcmp(token->string, "else")) {
        lexer_unget(token);
        return ast_if(cond, then, NULL);
    }

    last = parse_statement();
    return ast_if(cond, then, last);
}

static ast_t *parse_statement_declaration_semicolon(void) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, ';'))
        return NULL;
    lexer_unget(token);
    list_t *list = list_create();
    parse_statement_declaration(list);
    return list_shift(list);
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
    ast_localenv = table_create(ast_localenv);
    ast_t *init = parse_statement_declaration_semicolon();
    ast_t *cond = parse_expression_semicolon();
    ast_t *step = lexer_ispunct(lexer_peek(), ')') ? NULL : parse_expression();
    parse_expect(')');
    ast_t *body = parse_statement();
    ast_localenv = table_parent(ast_localenv);
    return ast_for(init, cond, step, body);
}

static ast_t *parse_statement_while(void) {
    parse_expect('(');
    ast_t *cond = parse_expression();
    parse_expect(')');
    ast_t *body = parse_statement();
    return ast_while(cond, body);
}

static ast_t *parse_statement_do(void) {
    ast_t         *body  = parse_statement();
    lexer_token_t *token = lexer_next();

    if (!parse_identifer_check(token, "while"))
        compile_error("expected while for do");

    parse_expect('(');
    ast_t *cond = parse_expression();
    parse_expect(')');
    parse_expect(';');

    return ast_do(cond, body);
}

static ast_t *parse_statement_break(void) {
    parse_expect(';');
    return ast_make(AST_TYPE_STATEMENT_BREAK);
}

static ast_t *parse_statement_continue(void) {
    parse_expect(';');
    return ast_make(AST_TYPE_STATEMENT_CONTINUE);
}

static ast_t *parse_statement_switch(void) {
    parse_expect('(');
    ast_t *expression = parse_expression();

    /* TODO lvalueness test propogate?*/

    parse_expect(')');
    ast_t *body = parse_statement();
    return ast_switch(expression, body);
}

static ast_t *parse_statement_case(void) {
    int value = parse_evaluate(parse_expression());
    parse_expect(':');
    return ast_case(value);
}

static ast_t *parse_statement_default(void) {
    parse_expect(':');
    return ast_make(AST_TYPE_STATEMENT_DEFAULT);
}

static ast_t *parse_statement_return(void) {
    ast_t *val = parse_expression();
    parse_expect(';');
    return ast_return(ast_data_table[AST_DATA_FUNCTION]->returntype, val);
}

static ast_t *parse_statement_goto(void) {
    lexer_token_t *token = lexer_next();
    if (!token || token->type != LEXER_TOKEN_IDENTIFIER)
        compile_error("expected identifier in goto statement");
    parse_expect(';');

    ast_t *node = ast_goto(token->string);
    list_push(ast_gotos, node);

    return node;
}

static void parse_label_backfill(void) {
    for (list_iterator_t *it = list_iterator(ast_gotos); !list_iterator_end(it); ) {
        ast_t *source      = list_iterator_next(it);
        char  *label       = source->gotostmt.label;
        ast_t *destination = table_find(ast_labels, label);

        if (!destination)
            compile_error("undefined label: %s", label);
        if (destination->gotostmt.where)
            source->gotostmt.where = destination->gotostmt.where;
        else
            source->gotostmt.where = destination->gotostmt.where = ast_label();
    }
}

static ast_t *parse_label(lexer_token_t *token) {
    parse_expect(':');
    char  *label = token->string;
    ast_t *node  = ast_label_new(label);

    if (table_find(ast_labels, label))
        compile_error("duplicate label: %s", label);
    table_insert(ast_labels, label, node);

    return node;
}

static ast_t *parse_statement(void) {
    lexer_token_t *token = lexer_next();
    ast_t         *ast;

    if (lexer_ispunct        (token, '{'))        return parse_statement_compound();
    if (parse_identifer_check(token, "if"))       return parse_statement_if();
    if (parse_identifer_check(token, "for"))      return parse_statement_for();
    if (parse_identifer_check(token, "while"))    return parse_statement_while();
    if (parse_identifer_check(token, "do"))       return parse_statement_do();
    if (parse_identifer_check(token, "return"))   return parse_statement_return();
    if (parse_identifer_check(token, "switch"))   return parse_statement_switch();
    if (parse_identifer_check(token, "case"))     return parse_statement_case();
    if (parse_identifer_check(token, "default"))  return parse_statement_default();
    if (parse_identifer_check(token, "break"))    return parse_statement_break();
    if (parse_identifer_check(token, "continue")) return parse_statement_continue();
    if (parse_identifer_check(token, "goto"))     return parse_statement_goto();

    if (token->type == LEXER_TOKEN_IDENTIFIER && lexer_ispunct(lexer_peek(), ':'))
        return parse_label(token);

    lexer_unget(token);

    ast = parse_expression();
    parse_expect(';');

    return ast;
}

static void parse_statement_declaration(list_t *list){
    lexer_token_t *token = lexer_peek();
    if (!token)
        compile_error("statement declaration with unexpected ending");
    if (parse_type_check(token))
        parse_declaration(list, ast_variable_local);
    else
        list_push(list, parse_statement());
}

static ast_t *parse_statement_compound(void) {
    ast_localenv = table_create(ast_localenv);
    list_t *statements = list_create();
    for (;;) {
        parse_statement_declaration(statements);
        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;

        lexer_unget(token);
    }
    ast_localenv = table_parent(ast_localenv);
    return ast_compound(statements);
}

static data_type_t *parse_function_parameters(list_t *paramvars, data_type_t *returntype) {
    bool           typeonly   = !paramvars;
    list_t        *paramtypes = list_create();
    lexer_token_t *token      = lexer_next();
    lexer_token_t *next       = lexer_next();

    if (parse_identifer_check(token, "void") && lexer_ispunct(next, ')'))
        return ast_prototype(returntype, paramtypes, false);
    lexer_unget(next);
    if (lexer_ispunct(token, ')'))
        return ast_prototype(returntype, paramtypes, true);
    lexer_unget(token);

    for (;;) {
        token = lexer_next();
        if (parse_identifer_check(token, "...")) {
            if (list_length(paramtypes) == 0)
                compile_error("ICE: %s (0)", __func__);
            parse_expect(')');
            return ast_prototype(returntype, paramtypes, true);
        } else {
            lexer_unget(token);
        }

        data_type_t *ptype;
        char        *name;
        parse_function_parameter(&ptype, &name, typeonly);
        parse_semantic_notvoid(ptype);
        if (ptype->type == TYPE_ARRAY)
            ptype = ast_pointer(ptype->pointer);
        list_push(paramtypes, ptype);

        if (!typeonly)
            list_push(paramvars, ast_variable_local(ptype, name));

        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, ')'))
            return ast_prototype(returntype, paramtypes, false);

        if (!lexer_ispunct(token, ','))
            compile_error("ICE: %s (2)", __func__);
    }
}

static ast_t *parse_function_definition(data_type_t *functype, char *name, list_t *parameters) {
    ast_localenv                      = table_create(ast_localenv);
    ast_locals                        = list_create();
    ast_data_table[AST_DATA_FUNCTION] = functype;

    ast_t *body = parse_statement_compound();
    ast_t *r    = ast_function(functype, name, parameters, body, ast_locals);

    table_insert(ast_globalenv, name, r);

    ast_data_table[AST_DATA_FUNCTION] = NULL;
    ast_localenv                      = NULL;
    ast_locals                        = NULL;

    return r;
}

static bool parse_function_definition_check(void) {
    list_t *buffer = list_create();
    int     nests  = 0;
    bool    paren  = false;
    bool    ready  = true;

    for (;;) {

        lexer_token_t *token = lexer_next();
        list_push(buffer, token);

        if (!token)
            compile_error("function definition with unexpected ending");

        if (nests == 0 && paren && lexer_ispunct(token, '{'))
            break;

        if (nests == 0 && (lexer_ispunct(token, ';')
                         ||lexer_ispunct(token, ',')
                         ||lexer_ispunct(token, '=')))
        {
            ready = false;
            break;
        }

        if (lexer_ispunct(token, '('))
            nests++;

        if (lexer_ispunct(token, ')')) {
            if (nests == 0)
                compile_error("unmatches parenthesis");
            paren = true;
            nests--;
        }
    }

    while (list_length(buffer) > 0)
        lexer_unget(list_pop(buffer));

    return ready;
}

static ast_t *parse_function_definition_intermediate(void) {
    data_type_t *basetype;
    char        *name;
    list_t      *parameters = list_create();

    basetype     = parse_declaration_specification(NULL);
    ast_localenv = table_create(ast_globalenv);
    ast_labels   = table_create(NULL);
    ast_gotos    = list_create();

    data_type_t *functype = parse_declarator(&name, basetype, parameters, CDECL_BODY);
    parse_expect('{');
    ast_t *value = parse_function_definition(functype, name, parameters);

    parse_label_backfill();

    ast_localenv = NULL;
    return value;
}

static data_type_t *parse_declarator_direct_restage(data_type_t *basetype, list_t *parameters) {
    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, '[')) {
        int length;
        token = lexer_next();
        if (lexer_ispunct(token, ']'))
            length = -1;
        else {
            lexer_unget(token);
            length = parse_evaluate(parse_expression());
            parse_expect(']');
        }

        data_type_t *type = parse_declarator_direct_restage(basetype, parameters);
        if (type->type == TYPE_FUNCTION)
            compile_error("array of functions");
        return ast_array(type, length);
    }
    if (lexer_ispunct(token, '(')) {
        if (basetype->type == TYPE_FUNCTION)
            compile_error("function returning function");
        if (basetype->type == TYPE_ARRAY)
            compile_error("function returning array");
        return parse_function_parameters(parameters, basetype);
    }
    lexer_unget(token);
    return basetype;
}

static void parse_qualifiers_skip(void) {
    for (;;) {
        lexer_token_t *token = lexer_next();
        if (parse_identifer_check(token, "const")
         || parse_identifer_check(token, "volatile")
         || parse_identifer_check(token, "restrict")) {
            continue;
        }
        lexer_unget(token);
        return;
    }
}

static data_type_t *parse_declarator_direct(char **rname, data_type_t *basetype, list_t *parameters, cdecl_t context) {
    lexer_token_t *token = lexer_next();
    lexer_token_t *next  = lexer_peek();

    if (lexer_ispunct(token, '(') && !parse_type_check(next) && !lexer_ispunct(next, ')')) {
        data_type_t *stub = ast_type_stub();
        data_type_t *type = parse_declarator_direct(rname, stub, parameters, context);
        parse_expect(')');
        *stub = *parse_declarator_direct_restage(basetype, parameters);
        return type;
    }

    if (lexer_ispunct(token, '*')) {
        parse_qualifiers_skip();
        data_type_t *stub = ast_type_stub();
        data_type_t *type = parse_declarator_direct(rname, stub, parameters, context);
        *stub = *ast_pointer(basetype);
        return type;
    }

    if (token->type == LEXER_TOKEN_IDENTIFIER) {
        if (context == CDECL_CAST)
            compile_error("wasn't expecting identifier `%s'", lexer_tokenstr(token));
        *rname = token->string;
        return parse_declarator_direct_restage(basetype, parameters);
    }

    if (context == CDECL_BODY || context == CDECL_PARAMETER)
        compile_error("expected identifier, `(` or `*` for declarator");

    lexer_unget(token);

    return parse_declarator_direct_restage(basetype, parameters);
}

static void parse_array_fix(data_type_t *type) {
    if (type->type == TYPE_ARRAY) {
        parse_array_fix(type->pointer);
        type->size = type->length * type->pointer->size;
    } else if (type->type == TYPE_POINTER) {
        parse_array_fix(type->pointer);
    } else if (type->type == TYPE_FUNCTION) {
        parse_array_fix(type->returntype);
    }
}

static data_type_t *parse_declarator(char **rname, data_type_t *basetype, list_t *parameters, cdecl_t context) {
    data_type_t *type = parse_declarator_direct(rname, basetype, parameters, context);
    parse_array_fix(type);
    return type;
}

static void parse_declaration(list_t *list, ast_t *(*make)(data_type_t *, char *)) {
    storage_t      storage;
    data_type_t   *basetype = parse_declaration_specification(&storage);
    lexer_token_t *token    = lexer_next();

    if (lexer_ispunct(token, ';'))
        return;

    lexer_unget(token);

    for (;;) {
        char        *name = NULL;
        data_type_t *type = parse_declarator(&name, basetype, NULL, CDECL_BODY);

        token = lexer_next();
        if (lexer_ispunct(token, '=')) {
            if (storage == STORAGE_TYPEDEF)
                compile_error("invalid use of typedef");
            parse_semantic_notvoid(type);
            ast_t *var = make(type, name);
            list_push(list, parse_declaration_initialization(var));
            token = lexer_next();
        } else if (storage == STORAGE_TYPEDEF) {
            table_insert(parse_typedefs, name, type);
        } else if (type->type == TYPE_FUNCTION) {
            make(type, name);
        } else {
            ast_t *var = make(type, name);
            if (storage != STORAGE_EXTERN)
                list_push(list, ast_declaration(var, NULL));
        }
        if (lexer_ispunct(token, ';'))
            return;
        if (!lexer_ispunct(token, ','))
            compile_error("Confused!");
    }
}

list_t *parse_run(void) {
    list_t *list = list_create();
    for (;;) {
        if (!lexer_peek())
            return list;
        if (parse_function_definition_check())
            list_push(list, parse_function_definition_intermediate());
        else
            parse_declaration(list, &ast_variable_global);
    }
    return NULL;
}
