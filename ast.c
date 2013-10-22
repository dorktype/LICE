#include <stdlib.h>
#include <string.h>

#include "gmcc.h"

static data_type_t *data_int  = &(data_type_t) { TYPE_INT,  NULL };
static data_type_t *data_char = &(data_type_t) { TYPE_CHAR, NULL };

static ast_t *ast_locals  = NULL;
static ast_t *ast_globals = NULL;

ast_t *ast_data_globals(void) { return ast_globals; }
ast_t *ast_data_locals(void) { return ast_locals; }

data_type_t *ast_data_int(void)  { return data_int;  }
data_type_t *ast_data_char(void) { return data_char; }

static int ast_label_index = 0;

// creates a new ast node
#define ast_new_node() \
    ((ast_t*)malloc(sizeof(ast_t)))

ast_t *ast_new_unary(char type, data_type_t *data, ast_t *operand) {
    ast_t *ast         = ast_new_node();
    ast->type          = type;
    ast->ctype         = data;
    ast->unary.operand = operand;

    return ast;
}

ast_t *ast_new_binary(char type, data_type_t *data, ast_t *left, ast_t *right) {
    ast_t *ast         = ast_new_node();
    ast->type          = type;
    ast->ctype         = data;
    ast->right         = right;
    ast->left          = left;

    return ast;
}

ast_t *ast_new_int(int value) {
    ast_t *ast   = ast_new_node();
    ast->type    = AST_TYPE_LITERAL;
    ast->ctype   = data_int;
    ast->integer = value;

    return ast;
}

ast_t *ast_new_char(char value) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_LITERAL;
    ast->ctype     = data_char;
    ast->character = value;

    return ast;
}

char *ast_new_label(void) {
    string_t *string = string_create();
    string_appendf(string, ".L%d", ast_label_index++);
    return string_buffer(string);
}

ast_t *ast_new_variable_local(data_type_t *type, char *name) {
    ast_t *ast      = ast_new_node();
    ast->type       = AST_TYPE_VAR_LOCAL;
    ast->ctype      = type;
    ast->local.name = name;
    ast->next       = NULL;

    if (ast_locals) {
        ast_t *item;
        for(item = ast_locals; item->next; item = item->next)
            ;

        item->next = ast;
    } else {
        ast_locals = ast;
    }

    return ast;
}

ast_t *ast_new_reference_local(data_type_t *type, ast_t *var, int off) {
    ast_t *ast         = ast_new_node();
    ast->type          = AST_TYPE_REF_LOCAL;
    ast->ctype         = type;
    ast->local_ref.ref = var;
    ast->local_ref.off = off;

    return ast;
}

ast_t *ast_new_variable_global(data_type_t *type, char *name, bool file) {
    ast_t *ast        = ast_new_node();
    ast->type         = AST_TYPE_VAR_GLOBAL;
    ast->ctype        = type;
    ast->global.name  = name;
    ast->global.label = (file) ? ast_new_label() : name;
    ast->next         = NULL;

    if (ast_globals) {
        ast_t *item;
        for (item = ast_globals; item->next; item = item->next)
            ;

        item->next  = ast;
    } else {
        ast_globals = ast;
    }

    return ast;
}

ast_t *ast_new_reference_global(data_type_t *type, ast_t *var, int off) {
    ast_t *ast          = ast_new_node();
    ast->type           = AST_TYPE_REF_GLOBAL;
    ast->ctype          = type;
    ast->global_ref.ref = var;
    ast->global_ref.off = off;

    return ast;
}

ast_t *ast_new_string(char *value) {
    ast_t *ast        = ast_new_node();
    ast->type         = AST_TYPE_STRING;
    ast->ctype        = ast_new_array(data_char, strlen(value) + 1);
    ast->string.data  = value;
    ast->string.label = ast_new_label();
    ast->next         = ast_globals;
    ast_globals       = ast;

    return ast;
}

ast_t *ast_new_call(char *name, int size, ast_t **args) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_CALL;
    ast->ctype     = data_int;
    ast->call.name = name;
    ast->call.size = size;
    ast->call.args = args;

    return ast;
}

ast_t *ast_new_decl(ast_t *var, ast_t *init) {
    ast_t *ast     = ast_new_node();
    ast->type      = AST_TYPE_DECL;
    ast->ctype     = NULL;
    ast->decl.var  = var;
    ast->decl.init = init;

    return ast;
}

ast_t *ast_new_array_init(int size, ast_t **init) {
    ast_t *ast      = ast_new_node();
    ast->type       = AST_TYPE_ARRAY_INIT;
    ast->ctype      = NULL;
    ast->array.size = size;
    ast->array.init = init;

    return ast;
}

data_type_t *ast_new_pointer(data_type_t *type) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_PTR;
    data->pointer     = type;

    return data;
}

data_type_t *ast_new_array(data_type_t *type, int size) {
    data_type_t *data = (data_type_t*)malloc(sizeof(data_type_t));
    data->type        = TYPE_ARRAY;
    data->pointer     = type;
    data->size        = size;

    return data;
}

ast_t *ast_find_variable(const char *name) {
    ast_t *ast;
    for (ast = ast_locals; ast; ast = ast->next)
        if (!strcmp(name, ast->local.name))
            return ast;
    for (ast = ast_globals; ast; ast = ast->next)
        if (!strcmp(name, ast->global.name))
            return ast;

    return NULL;
}

const char *ast_type_string(data_type_t *type) {
    string_t *string;

    switch (type->type) {
        case TYPE_VOID: return "void";
        case TYPE_INT:  return "int";
        case TYPE_CHAR: return "char";

        case TYPE_PTR:
            string = string_create();
            string_appendf(string, "%s*", ast_type_string(type->pointer));
            return string_buffer(string);

        case TYPE_ARRAY:
            string = string_create();
            string_appendf(
                string,
                "%s[%d]",
                ast_type_string(type->pointer),
                type->size
            );
            return string_buffer(string);
    }
    return NULL;
}

static void ast_dump_string_impl(string_t *string, ast_t *ast) {
    char *left;
    char *right;

    size_t i;
    if (!ast) return;
    switch (ast->type) {
        case AST_TYPE_LITERAL:
            switch (ast->ctype->type) {
                case TYPE_INT:
                    string_appendf(string, "%d", ast->integer);
                    break;
                case TYPE_CHAR:
                    string_appendf(string, "'%c'", ast->character);
                    break;
                default:
                    compile_error("Internal error %s", __func__);
                    break;
            }
            break;

        case AST_TYPE_STRING:
            string_appendf(string, "\"%s\"", string_quote(ast->string.data));
            break;

        case AST_TYPE_VAR_LOCAL:
            string_appendf(string, "%s", ast->local.name);
            break;
        case AST_TYPE_VAR_GLOBAL:
            string_appendf(string, "%s", ast->global.name);
            break;

        case AST_TYPE_REF_LOCAL:
            string_appendf(string, "%s[%d]", ast_dump_string(ast->local_ref.ref), ast->local_ref.off);
            break;
        case AST_TYPE_REF_GLOBAL:
            string_appendf(string, "%s[%d]", ast_dump_string(ast->global_ref.ref), ast->global_ref.off);
            break;

        case AST_TYPE_CALL:
            string_appendf(string, "%s(", ast->call.name);
            for(i = 0; i < ast->call.size; i++) {
                ast_dump_string_impl(string, ast->call.args[i]);
                if (ast->call.args[i + 1])
                    string_appendf(string, ",");
            }
            string_appendf(string, ")");
            break;


        case AST_TYPE_ARRAY_INIT:
            string_appendf(string, "{");
            for (i = 0; i < ast->array.size; i++) {
                ast_dump_string_impl(string, ast->array.init[i]);
                if (i != ast->array.size - 1)
                    string_appendf(string, ",");
            }
            string_appendf(string, "}");
            break;

        case AST_TYPE_ADDR:
            string_appendf(string, "(& %s)", ast_dump_string(ast->unary.operand));
            break;
        case AST_TYPE_DEREF:
            string_appendf(string, "(* %s)", ast_dump_string(ast->unary.operand));
            break;

        case AST_TYPE_DECL:
            string_appendf(
                string,
                "(decl %s %s %s)",
                ast_type_string(ast->decl.var->ctype),
                ast->decl.var->local.name,
                ast_dump_string(ast->decl.init)
            );
            break;

        default:
            left  = ast_dump_string(ast->left);
            right = ast_dump_string(ast->right);
            string_appendf(string, "(%c %s %s)", ast->type, left, right);
            break;
    }
}

char *ast_dump_string(ast_t *ast) {
    string_t *string = string_create();
    ast_dump_string_impl(string, ast);
    return string_buffer(string);
}
