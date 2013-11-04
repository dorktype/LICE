#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "lice.h"

#define PARSE_BUFFER    1024
#define PARSE_CALLS     6    // only six registers to use for amd64
#define PARSE_ALIGNMENT 16

static ast_t *parse_expression(void);
static ast_t *parse_expression_unary(void);
static ast_t *parse_statement_compound(void);
static ast_t *parse_statement_declaration(void);
static ast_t *parse_statement(void);
static void   parse_declaration_intermediate(char **name, data_type_t **ctype, storage_t *storage);
static data_type_t *parse_declarator(data_type_t *basetype);

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

// the following is a feature complete evaluator via recursiveness, yes the
// ast nodes are prpagated .. RECURSIVELY until a value is assumed.
static int parse_evaluate(ast_t *ast) {
    switch (ast->type) {
        case AST_TYPE_LITERAL:
            if (ast_type_integer(ast->ctype))
                return ast->integer;
            compile_error("Not a valid integer constant expression");
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

        // unary is special
        case '!': return !parse_evaluate(ast->unary.operand);
        case '~': return ~parse_evaluate(ast->unary.operand);

        default:
            compile_error("Not a valid integer constant expression");
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
        compile_error("Too few arguments: %s", name);
    for (list_iterator_t *it = list_iterator(parameters),
                         *jt = list_iterator(arguments); !list_iterator_end(jt); )
    {
        data_type_t *parameter = list_iterator_next(it);
        data_type_t *argument  = list_iterator_next(jt);

        // handle implicit int
        if (parameter)
            ast_result_type('=', parameter, argument);
        else
            ast_result_type('=', argument, ast_data_int);
    }
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

    ast_t *func = table_find(ast_localenv, name);
    if (func) {
        data_type_t *declaration = func->ctype;
        if (declaration->type != TYPE_FUNCTION)
            compile_error("%s isn't a function fool!\n", name);
        parse_function_typecheck(name, declaration->parameters, parse_parameter_types(list));
        return ast_new_call(declaration->returntype, name, list, declaration->parameters);
    }

    return ast_new_call(ast_data_int, name, list, list_create());
}


static ast_t *parse_generic(char *name) {
    ast_t         *var   = NULL;
    lexer_token_t *token = lexer_next();

    if (lexer_ispunct(token, '('))
        return parse_function_call(name);

    lexer_unget(token);

    if (!(var = table_find(ast_localenv, name)))
        compile_error("Undefined variable: %s", name);

    return var;
}

static ast_t *parse_number(char *string) {
    char *p    = string;
    int   base = 10;

    if (*p == '0') {
        p++;
        if (*p == 'x' || *p == 'X') {
            base = 16;
            p++;
        } else if (isdigit(*p)) {
            base = 8; // octal
        }
    }

    char *start = p;
    while (isxdigit(*p))
        p++;

    if (*p == '.') {
        if (base != 10)
            compile_error("Malformatted numerical value (1)");
        p++;

        while (isdigit(*p))
            p++;

        if (*p != '\0')
            compile_error("Malformmated numerical value (2)");

        return ast_new_floating(atof(start));
    }
    if (!strcasecmp(p, "l"))
        return ast_new_integer(ast_data_long, strtol(start, NULL, base));
    else if (!strcasecmp(p, "ul") || !strcasecmp(p, "lu"))
        return ast_new_integer(ast_data_ulong, strtoul(start, NULL, base));
    else {
        if (*p != '\0')
            compile_error("Malformatted numerical value (3) %s (%s)", string, p);
        long test = strtol(start, NULL, base);
        if (test & ~(long)UINT_MAX)
            return ast_new_integer(ast_data_long, test);
        return ast_new_integer(ast_data_int, test);
    }
    return NULL;
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
        case LEXER_TOKEN_NUMBER:
            return parse_number(token->string);

        case LEXER_TOKEN_CHAR:
            return ast_new_integer(ast_data_char, token->character);

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
    // generates the following code for subscript
    // (deref (+ ast subscript))
    ast_t *subscript = parse_expression();
    parse_expect(']');
    ast_t *node = ast_new_binary('+', ast, subscript);
    return ast_new_unary(AST_TYPE_DEREFERENCE, node->ctype->pointer, node);
}

static ast_t *parse_sizeof(bool typename) {
    lexer_token_t *token = lexer_next();
    if (typename && parse_type_check(token)) {
        lexer_unget(token);

        char        *ignore;
        storage_t    storage;
        data_type_t *type = NULL;

        parse_declaration_intermediate(&ignore, &type, &storage);

        return ast_new_integer(ast_data_long, type->size);
    }
    // deal with the sizeof () thing
    if (lexer_ispunct(token, '(')) {
        ast_t *next = parse_sizeof(true);
        parse_expect(')');
        return next;
    }

    lexer_unget(token);
    ast_t *expression = parse_expression_unary();
    if (expression->ctype->size == 0)
        compile_error("sizeof void makes no sense!");
    return ast_new_integer(ast_data_long, expression->ctype->size);
}

static ast_t *parse_expression_unary(void) {
    lexer_token_t *token = lexer_next();

    if (!token)
        compile_error("internal error");

    if (parse_identifer_check(token, "sizeof")) {
        return parse_sizeof(false);
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
    if (lexer_ispunct(token, '-')) {
        ast_t *ast = parse_expression();
        return ast_new_binary('-', ast_new_integer(ast_data_int, 0), ast);
    }
    if (lexer_ispunct(token, '~')) {
        ast_t *ast = parse_expression();
        if (!ast_type_integer(ast->ctype))
            compile_error("Internal error: parse_expression_unary (1)");
        return ast_new_unary('~', ast->ctype, ast);
    }
    if (lexer_ispunct(token, '*')) {
        ast_t       *operand = parse_expression_unary();
        data_type_t *type    = ast_array_convert(operand->ctype);

        if (type->type != TYPE_POINTER)
            compile_error("Internal error: parse_expression_unary (2)");

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

    data_type_t *field = table_find(structure->ctype->fields, name->string);
    return ast_structure_reference_new(field, structure, name->string);
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
        if (lexer_ispunct(token, '^')
        ||  lexer_ispunct(token, '%')
        ||  lexer_ispunct(token, LEXER_TOKEN_LSHIFT)
        ||  lexer_ispunct(token, LEXER_TOKEN_RSHIFT))
        {
            if (!ast_type_integer(ast->ctype) ||
                !ast_type_integer(next->ctype)) {

                // eh?
                compile_error(
                    "Not a valid expression: %s ^ %s (types %s and %s)",
                    ast_string(ast),
                    ast_string(next),
                    ast_type_string(ast->ctype),
                    ast_type_string(next->ctype)
                );
            }
        }
        ast = ast_new_binary(token->punct, ast, next);
    }
    return NULL;
}

static ast_t *parse_expression(void) {
    return parse_expression_intermediate(16);
}

static bool parse_type_check(lexer_token_t *token) {
    if (token->type != LEXER_TOKEN_IDENT)
        return false;

    static const char *keywords[] = {
        "char",   "short", "int",    "long",     "float", "double",
        "struct", "union", "signed", "unsigned", "enum",  "void",
        "extern", "typedef"
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
        compile_error("Expected expression");

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
            ast_t *value = ast_new_integer(ast_data_char, *p);
            value->initlist.type = ast_data_char;
            list_push(initlist, value);
        }
        ast_t *value = ast_new_integer(ast_data_char, '\0');
        value->initlist.type = ast_data_char;
        list_push(initlist, value);
        return;
    }

    if (!lexer_ispunct(token, '{'))
        compile_error("Expected initializer list");

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
    ast_t *init = ast_new_initializerlist(initlist);

    int length = (init->type == AST_TYPE_STRING)
                    ? strlen(init->string.data)
                    : list_length(init->initlist.list);

    if (type->length == -1) {
        type->length = length;
        type->size   = length * type->pointer->size;
    } else if (type->length != length) {
        compile_error("Internal error");
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
            return ast_new_initializerlist(initlist);

        if (lexer_ispunct(token, '{')) {
            if (fieldtype->type != TYPE_ARRAY)
                compile_error("Internal error");
            lexer_unget(token);
            parse_declaration_array_initializer_intermediate(initlist, fieldtype);
            continue;
        }

        lexer_unget(token);
        parse_declaration_initializerlist_element(initlist, fieldtype);
    }
    parse_expect('}');
    return ast_new_initializerlist(initlist);
}

// parses a union/structure tag
static char *parse_memory_tag(void) {
    lexer_token_t *token = lexer_next();
    if (token->type == LEXER_TOKEN_IDENT)
        return token->string;
    lexer_unget(token);
    return NULL;
}

static table_t *parse_memory_fields(void) {
    lexer_token_t *token = lexer_next();
    if (!lexer_ispunct(token, '{')) {
        lexer_unget(token);
        return NULL;
    }

    table_t *table = table_create(NULL);
    for (;;) {
        if (!parse_type_check(lexer_peek()))
            break;

        char        *name;
        storage_t    storage;
        data_type_t *type = NULL;

        parse_declaration_intermediate(&name, &type, &storage);

        table_insert(table, name, ast_structure_field_new(type, 0));
        parse_expect(';');
    }
    parse_expect('}');
    return table;
}

static int parse_union_size(table_t *fields) {
    int maxsize = 0;
    for (list_iterator_t *it = list_iterator(table_values(fields)); !list_iterator_end(it); ) {
        data_type_t *type = list_iterator_next(it);
        maxsize = (maxsize < type->size)
                    ? type->size
                    : maxsize;
    }
    return maxsize;
}

static int parse_structure_size(table_t *fields) {
    int offset = 0;
    for (list_iterator_t *it = list_iterator(table_values(fields)); !list_iterator_end(it); ) {
        data_type_t *type = list_iterator_next(it);
        type->offset = offset;
        offset += type->size;
    }
    return offset;
}

static data_type_t *parse_tag_definition(table_t *table, int (*compute)(table_t *table)) {
    char        *tag    = parse_memory_tag();
    data_type_t *prev   = tag ? table_find(ast_unions, tag) : NULL;
    table_t     *fields = parse_memory_fields();

    if (prev && !fields)
        return prev;

    if (prev && fields) {
        prev->fields = fields;
        prev->size   = compute(fields);
        return prev;
    }

    data_type_t *r = (fields)
        ? ast_structure_new(fields, compute(fields))
        : ast_structure_new(NULL,   0);

    if (tag)
        table_insert(ast_unions, tag, r);

    return r;
}

static data_type_t *parse_enumeration(void) {
    lexer_token_t *token = lexer_next();
    if (token->type == LEXER_TOKEN_IDENT)
        token = lexer_next();
    if (!lexer_ispunct(token, '{')) {
        lexer_unget(token);
        return ast_data_int;
    }
    int accumulate = 0;
    for (;;) {
        token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;

        if (token->type != LEXER_TOKEN_IDENT)
            compile_error("NOPE");

        char *name = token->string;
        token = lexer_next();
        if (lexer_ispunct(token, '='))
            accumulate = parse_evaluate(parse_expression());
        else
            lexer_unget(token);

        ast_t *constval = ast_new_integer(ast_data_int, accumulate++);
        table_insert(ast_localenv ? ast_localenv : ast_globalenv, name, constval);
        token = lexer_next();
        if (lexer_ispunct(token, ','))
            continue;
        if (lexer_ispunct(token, '}'))
            break;

        compile_error("NOPE!");
    }
    return ast_data_int;
}

static void parse_declaration_specification(data_type_t **rtype, storage_t *storage) {
    *rtype   = NULL;
    *storage = 0;

    lexer_token_t *token = lexer_peek();
    if (!token || token->type != LEXER_TOKEN_IDENT)
        return;

    // large declaration specification state machine
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
            if (STATE == 0) {                                          \
                goto state_machine_error;                              \
            }                                                          \
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
            set_check(*storage, VALUE);                                \
        } while (0)

    #define state_machine_try(THING) \
        if (!strcmp(token->string, THING))

    for (;;) {
        token = lexer_next();
        if (!token)
            compile_error("ICE");

        if (token->type != LEXER_TOKEN_IDENT) {
            lexer_unget(token);
            break;
        }

             state_machine_try("typedef")  set_class(STORAGE_TYPEDEF);
        else state_machine_try("extern")   set_class(STORAGE_EXTERN);
        else state_machine_try("static")   set_class(STORAGE_STATIC);
        else state_machine_try("auto")     set_class(STORAGE_AUTO);
        else state_machine_try("register") set_class(STORAGE_REGISTER);
        else state_machine_try("const")    kconst    = true;
        else state_machine_try("volatile") kvolatile = true;
        else state_machine_try("inline")   kinline   = true;
        else state_machine_try("void")     set_state(type,      kvoid);
        else state_machine_try("char")     set_state(type,      kchar);
        else state_machine_try("int")      set_state(type,      kint);
        else state_machine_try("float")    set_state(type,      kfloat);
        else state_machine_try("double")   set_state(type,      kdouble);
        else state_machine_try("signed")   set_state(signature, ksigned);
        else state_machine_try("unsigned") set_state(signature, kunsigned);
        else state_machine_try("short")    set_state(size,      kshort);
        else state_machine_try("struct")   set_state(user,      parse_tag_definition(ast_structures, &parse_structure_size));
        else state_machine_try("union")    set_state(user,      parse_tag_definition(ast_unions,     &parse_union_size));
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

    if (user) {
        *rtype = user;
        return;
    }

    switch (type) {
        case kchar:
            *rtype = ast_type_create(TYPE_CHAR,  signature != kunsigned);
            return;
        case kfloat:
            *rtype = ast_type_create(TYPE_FLOAT, false);
            return;
        case kdouble:
            *rtype = ast_type_create(
                (size == klong)
                    ? TYPE_DOUBLE
                    : TYPE_LDOUBLE,
                false
            );
            return;
        default:
            break;
    }

    switch (size) {
        case kshort:
            *rtype = ast_type_create(TYPE_SHORT, signature != kunsigned);
            return;
        case klong:
            *rtype = ast_type_create(TYPE_LONG,  signature != kunsigned);
            return;
        case kllong:
            *rtype = ast_type_create(TYPE_LLONG, signature != kunsigned);

        default:
            // retarded implicit int becomes easy this way, at least on of the
            // useful benefits of such a complicated state machine.
            *rtype = ast_type_create(TYPE_INT,   signature != kunsigned);
            return;
    }

    compile_error("ICE (BAD)");
state_machine_error:
    compile_error("ICE (GOOD)");
}

static ast_t *parse_declaration_initialization_value(data_type_t *type) {
    ast_t *init = (type->type == TYPE_ARRAY)
                        ? parse_declaration_array_initializer_value(type)
                        : (type->type == TYPE_STRUCTURE)
                                ? parse_declaration_structure_initializer_value(type)
                                : parse_expression();
    parse_expect(';');
    return init;
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
        return ast_new_array(next, dimension);
    }
    return ast_new_array(basetype, dimension);
}

static data_type_t *parse_array_dimensions(data_type_t *basetype) {
    data_type_t *data = parse_array_dimensions_intermediate(basetype);
    return (data) ? data : basetype;
}

static ast_t *parse_declaration_initialization(ast_t *var) {
    ast_t *init = parse_declaration_initialization_value(var->ctype);
    if (var->type == AST_TYPE_VAR_GLOBAL && ast_type_integer(var->ctype))
        init = ast_new_integer(ast_data_int, parse_evaluate(init));
    return ast_new_decl(var, init);
}

static void parse_declaration_intermediate(char **name, data_type_t **ctype, storage_t *storage) {
    data_type_t *basetype;
    parse_declaration_specification(&basetype, storage);

    data_type_t   *type  = parse_declarator(basetype);
    lexer_token_t *token = lexer_next();

    if (lexer_ispunct(token, ';')) {
        lexer_unget(token);
        *name = NULL;
        return;
    }

    if (token->type != LEXER_TOKEN_IDENT) {
        lexer_unget(token);
        *name = NULL;
    } else {
        *name = token->string;
    }

    *ctype = parse_array_dimensions(type);
}

static ast_t *parse_declaration(void) {
    char        *name;
    data_type_t *type;
    storage_t    storage;

    parse_declaration_intermediate(&name, &type, &storage);

    if (!name) {
        parse_expect(';');
        return NULL;
    }

    if (storage == STORAGE_TYPEDEF) {
        table_insert(parse_typedefs, name, type);
        parse_expect(';');
        return NULL;
    }

    ast_t         *var   = ast_new_variable_local(type, name);
    lexer_token_t *token = lexer_next();

    if (lexer_ispunct(token, '='))
        return parse_declaration_initialization(var);

    lexer_unget(token);
    parse_expect(';');

    return ast_new_decl(var, NULL);
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
    ast_localenv = table_create(ast_localenv);
    ast_t *init = parse_statement_declaration_semicolon();
    ast_t *cond = parse_expression_semicolon();
    ast_t *step = lexer_ispunct(lexer_peek(), ')') ? NULL : parse_expression();
    parse_expect(')');
    ast_t *body = parse_statement();
    ast_localenv = table_parent(ast_localenv);
    return ast_new_for(init, cond, step, body);
}

static ast_t *parse_statement_return(void) {
    ast_t *val = parse_expression();
    parse_expect(';');
    return ast_new_return(ast_data_function->returntype, val); // todo more accurte type
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
    for (;;) {
        lexer_token_t *token = lexer_peek();
        if (!token)
            compile_error("ICE");
        return parse_type_check(token) ? parse_declaration() : parse_statement();
    }
}

static ast_t *parse_statement_compound(void) {
    ast_localenv = table_create(ast_localenv);
    list_t *statements = list_create();
    for (;;) {
        ast_t *statement = parse_statement_declaration();
        if (statement)
            list_push(statements, statement);

        if (!statement)
            continue;

        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, '}'))
            break;

        lexer_unget(token);
    }
    ast_localenv = table_parent(ast_localenv);
    return ast_new_compound(statements);
}

static void parse_function_parameters(data_type_t **rtype, list_t *paramvars, data_type_t *returntype) {
    bool           typeonly   = !paramvars;
    list_t        *paramtypes = list_create();
    lexer_token_t *token      = lexer_next();

    if (lexer_ispunct(token, ')')) {
        *rtype = ast_new_prototype(returntype, paramtypes, false);
        return;
    }

    lexer_unget(token);
    for (;;) {
        token = lexer_next();
        if (parse_identifer_check(token, "...")) {
            if (list_length(paramtypes) == 0)
                compile_error("ICE: %s (0)", __func__);
            parse_expect(')');
            *rtype = ast_new_prototype(returntype, paramtypes, true);
            return;
        } else {
            lexer_unget(token);
        }

        // specification basetype
        data_type_t   *basetype;
        storage_t      storage;
        parse_declaration_specification(&basetype, &storage);

        data_type_t   *type     = parse_declarator(basetype);
        lexer_token_t *name     = lexer_next();

        if (name->type != LEXER_TOKEN_IDENT) {
            if (!typeonly)
                compile_error("ICE: %s (1) [%s]", __func__, lexer_tokenstr(name));
            lexer_unget(name);
            name = NULL;
        }

        type = parse_array_dimensions(type);
        if (type->type == TYPE_ARRAY)
            type = ast_new_pointer(type->pointer);

        list_push(paramtypes, type);
        if (!typeonly)
            list_push(paramvars, ast_new_variable_local(type, name->string));

        lexer_token_t *token = lexer_next();
        if (lexer_ispunct(token, ')')) {
            *rtype = ast_new_prototype(returntype, paramtypes, false);
            return;
        }

        if (!lexer_ispunct(token, ','))
            compile_error("ICE: %s (2)", __func__);
    }
}

static ast_t *parse_function_definition(data_type_t *functype, char *name, list_t *parameters) {
    ast_localenv      = table_create(ast_localenv);
    ast_locals        = list_create();
    ast_data_function = functype;

    ast_t *body = parse_statement_compound();
    ast_t *r    = ast_new_function(functype, name, parameters, body, ast_locals);

    table_insert(ast_globalenv, name, r);

    ast_data_function = NULL;
    ast_localenv      = NULL;
    ast_locals        = NULL;

    return r;
}

static ast_t *parse_function_declaration_definition(data_type_t *returntype, char *name) {
    ast_localenv = table_create(ast_globalenv);

    // parse function prototypes
    data_type_t   *functype;
    list_t        *parameters = list_create();
    parse_function_parameters(&functype, parameters, returntype);

    lexer_token_t *token = lexer_next();
    if (lexer_ispunct(token, '{'))
        return parse_function_definition(functype, name, parameters);
    table_insert(ast_globalenv, name, ast_new_variable_global(functype, name));
    return NULL;
}

static data_type_t *parse_declarator(data_type_t *base) {
    data_type_t *type = base;
    for (;;) {
        lexer_token_t *token = lexer_next();
        if (parse_identifer_check(token, "const"))
            continue; // ignore const for now
        if (!lexer_ispunct(token, '*')) {
            lexer_unget(token);
            return type;
        }
        type = ast_new_pointer(type);
    }
    return NULL;
}

list_t *parse_run(void) {
    list_t *list = list_create();
    for (;;) {
        // need a way to drop out
        lexer_token_t *get = lexer_next();
        if (!get)
            return list;

        // ignore static and const storage for now
        if (parse_identifer_check(get, "static") || parse_identifer_check(get, "const"))
            continue;


        lexer_unget(get); // stage back

        // deal with specification
        data_type_t   *base;
        storage_t      storage;
        parse_declaration_specification(&base, &storage);

        data_type_t   *type  = parse_declarator(base);
        lexer_token_t *token = lexer_next();

        if (lexer_ispunct(token, ';'))
            continue;

        if (token->type != LEXER_TOKEN_IDENT)
            compile_error("Internal error");

        type = parse_array_dimensions(type);
        lexer_token_t *peek = lexer_next();
        if (lexer_ispunct(peek, '=')) {
            if (storage == STORAGE_TYPEDEF)
                compile_error("invalid use of typedef");
            list_push(list, parse_declaration_initialization(ast_new_variable_global(type, token->string)));
            continue;
        }

        if (lexer_ispunct(peek, '(')) {
            ast_t *func = NULL;
            switch (storage) {
                case STORAGE_EXTERN:
                    parse_function_parameters(&type, NULL, type);
                    parse_expect(';');
                    ast_new_variable_global(type, token->string);
                    break;

                case STORAGE_TYPEDEF:
                    parse_function_parameters(&type, NULL, type);
                    parse_expect(';');
                    table_insert(parse_typedefs, token->string, type);
                    break;

                default:
                    func = parse_function_declaration_definition(type, token->string);
                    if (func)
                        list_push(list, func);
                    break;
            }
            continue;
        }

        if (lexer_ispunct(peek, ';')) {
            if (storage == STORAGE_TYPEDEF)
                table_insert(parse_typedefs, token->string, type);
            else {
                ast_t *var = ast_new_variable_global(type, token->string);
                if (storage != STORAGE_EXTERN)
                    list_push(list, ast_new_decl(var, NULL));
            }
            continue;
        }
        compile_error("Confused!\n");
    }
    return NULL;
}
