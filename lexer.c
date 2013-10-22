#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gmcc.h"

// an efficent strechy buffer string
struct string_s {
    char *buffer;
    int   allocated;
    int   length;
};

string_t *string_create(void) {
    string_t *string  = (string_t*)malloc(sizeof(string_t));
    string->buffer    = malloc(16);
    string->allocated = 16;
    string->length    = 0;
    string->buffer[0] = '\0';
    return string;
}

static void string_reallocate(string_t *string) {
    int   size   = string->allocated * 2;
    char *buffer = malloc(size);

    strcpy(buffer, string->buffer);
    string->buffer    = buffer;
    string->allocated = size;
}

char *string_buffer(string_t *string) {
    return string->buffer;
}

void string_append(string_t *string, char ch) {
    if (string->allocated == (string->length + 1))
        string_reallocate(string);
    string->buffer[string->length++] = ch;
    string->buffer[string->length]   = '\0';
}

char *string_quote(char *p) {
    string_t *string = string_create();
    while (*p) {
        if (*p == '\"' || *p == '\\')
            string_append(string, '\\');
        string_append(string, *p);
        p++;
    }
    return string->buffer;
}

void string_appendf(string_t *string, const char *fmt, ...) {
    va_list  va;
    for (;;) {
        int left  = string->allocated - string->length;
        int write;

        va_start(va, fmt);
        write = vsnprintf(string->buffer + string->length, left, fmt, va);
        va_end(va);

        if (left < write) {
            string_reallocate(string);
            continue;
        }
        string->length += write;
        return;
    }
}

// now the lexer
#define lexer_token_new() \
    ((lexer_token_t*)malloc(sizeof(lexer_token_t)))

static lexer_token_t *lexer_ungotton = NULL;


static lexer_token_t *lexer_identifier(string_t *str) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_IDENT;
    token->string        = string_buffer(str);
    return token;
}
static lexer_token_t *lexer_strtok(string_t *str) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_STRING;
    token->string        = string_buffer(str);
    return token;
}
static lexer_token_t *lexer_punct(char punct) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_PUNCT;
    token->punct         = punct;
    return token;
}
static lexer_token_t *lexer_int(int value) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_INT;
    token->integer       = value;
    return token;
}
static lexer_token_t *lexer_char(char value) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_CHAR;
    token->character     = value;
    return token;
}

// skip whitespace
static int lexer_skip(void) {
    int c;
    while ((c = getc(stdin)) != EOF) {
        if (isspace(c) || c == '\n' || c == '\r')
            continue;
        ungetc(c, stdin);
        return c;
    }
    return EOF;
}

// read a number and build integer token for the token stream
static lexer_token_t *lexer_read_number(int c) {
    int n = c - '0';
    for (;;) {
        int p = getc(stdin);
        if (!isdigit(p)) {
            ungetc(p, stdin);
            return lexer_int(n);
        }
        n = n * 10 + (p - '0');
    }
    return NULL;
}

// read a character and build character token for the token stream
static lexer_token_t *lexer_read_character(void) {
    int c1 = getc(stdin);
    int c2;

    // sanity
    if (c1 == EOF)
        goto lexer_read_character_error;
    if (c1 == '\\' && ((c1 = getc(stdin)) == EOF))
        goto lexer_read_character_error;

    if ((c2 = getc(stdin)) == EOF)
        goto lexer_read_character_error;
    if (c2 != '\'')
        compile_error("Malformatted character literal");

    return lexer_char(c1);

lexer_read_character_error:
    compile_error("Expected termination for character literal");
    return NULL;
}

// read a string and build string token for the token stream
static lexer_token_t *lexer_read_string(void) {
    string_t *string = string_create();
    for (;;) {
        int c = getc(stdin);
        if (c == EOF)
            compile_error("Expected termination for string literal");

        if (c == '"')
            break;

        if (c == '\\' && ((c = getc(stdin)) == EOF))
            compile_error("Expected termination in line continuation for string literal");

        string_append(string, c); // append character
    }
    return lexer_strtok(string);
}

// read an indentifier and build identifier token for the token stream
static lexer_token_t *lexer_read_identifier(int c1) {
    string_t *string = string_create();
    string_append(string, (char)c1);

    for (;;) {
        int c2 = getc(stdin);

        // underscores are allowed in identifiers, as is
        // $ (GNU extension)
        if (isalnum(c2) || c2 == '_' || c2 == '$') {
            string_append(string, c2);
        } else {
            ungetc(c2, stdin);
            return lexer_identifier(string);
        }
    }
    return NULL;
}

// read an token and build a token for the token stream
static lexer_token_t *lexer_read_token(void) {
    int c;
    lexer_skip();

    switch ((c = getc(stdin))) {
        case '0' ... '9':  return lexer_read_number(c);
        case '"':          return lexer_read_string();
        case '\'':         return lexer_read_character();

        // identifiers
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '$':
        case '_':
            return lexer_read_identifier(c);

        // punctuation
        case '+': case '-': case '/': case '*': case '=':
        case '(': case ')':
        case '[': case ']': // todo
        case '{': case '}': // todo
        case ',': case ';':
        case '&':
            return lexer_punct(c);

        case EOF:
            return NULL;

        default:
            compile_error("Unexpected character: `%c`", c);
    }

    // never reached
    return NULL;
}

bool lexer_ispunc(lexer_token_t *token, char c) {
    if (!token)
        compile_error("Internal error %s", __func__);
    return token->type == LEXER_TOKEN_PUNCT && token->punct == c;
}

void lexer_unget(lexer_token_t *token) {
    if (lexer_ungotton)
        compile_error("Internal error: OOM");
    lexer_ungotton = token;
}

lexer_token_t *lexer_next(void) {
    if (lexer_ungotton) {
        lexer_token_t *temp = lexer_ungotton;
        lexer_ungotton      = NULL;
        return temp;
    }

    return lexer_read_token();
}

lexer_token_t *lexer_peek(void) {
    lexer_token_t *token = lexer_next();
    lexer_unget(token);
    return token;
}

char *lexer_tokenstr(lexer_token_t *token) {
    string_t *string;
    switch (token->type) {
        // overlaps same memory
        case LEXER_TOKEN_PUNCT:
        case LEXER_TOKEN_CHAR:
            string = string_create();
            string_append(string, token->character);
            return string_buffer(string);

        case LEXER_TOKEN_INT:
            string = string_create();
            string_appendf(string, "%d", token->integer);
            return string_buffer(string);

        case LEXER_TOKEN_STRING:
            string = string_create();
            string_appendf(string, "\"%s\"", token->string);
            return string_buffer(string);

        case LEXER_TOKEN_IDENT:
            return token->string;
    }
    compile_error("Internal error: unexpected token");
    return NULL;
}
