#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lice.h"
#include "util.h"

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
static lexer_token_t *lexer_punct(int punct) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_PUNCT;
    token->punct         = punct;
    return token;
}
static lexer_token_t *lexer_number(char *string) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_NUMBER;
    token->string        = string;
    return token;
}
static lexer_token_t *lexer_char(char value) {
    lexer_token_t *token = lexer_token_new();
    token->type          = LEXER_TOKEN_CHAR;
    token->character     = value;
    return token;
}

static void lexer_skip_comment_line(void) {
    for (;;) {
        int c = getc(stdin);
        if (c == '\n' || c == EOF)
            return;
    }
}

static void lexer_skip_comment_block(void) {
    enum {
        comment_outside,
        comment_astrick
    } state = comment_outside;

    for (;;) {
        int c = getc(stdin);
        if (c == '*')
            state = comment_astrick;
        else if (state == comment_astrick && c == '/')
            return;
        else
            state = comment_outside;
    }
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
    string_t *string = string_create();
    string_cat(string, c);
    for (;;) {
        int p = getc(stdin);
        if (!isdigit(p) && !isalpha(p) && p != '.') {
            ungetc(p, stdin);
            return lexer_number(string_buffer(string));
        }
        string_cat(string, p);
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

        // handle continuation and quote continuation correctly
        // I think.
        if (c == '\\') {
            c = getc(stdin);
            switch (c) {
                case EOF:
                    compile_error("Unterminated `\\`");
                    break;

                case '\"':
                    break;

                case 'n':
                    c = '\n';
                    break;

                default:
                    compile_error("Unknown quote: %c", c);
                    break;
            }
        }

        string_cat(string, c); // append character
    }
    return lexer_strtok(string);
}

// read an indentifier and build identifier token for the token stream
static lexer_token_t *lexer_read_identifier(int c1) {
    string_t *string = string_create();
    string_cat(string, (char)c1);

    for (;;) {
        int c2 = getc(stdin);

        // underscores are allowed in identifiers, as is
        // $ (GNU extension)
        if (isalnum(c2) || c2 == '_' || c2 == '$') {
            string_cat(string, c2);
        } else {
            ungetc(c2, stdin);
            return lexer_identifier(string);
        }
    }
    return NULL;
}

static lexer_token_t *lexer_read_reclassify(int expect, int a, int b) {
    int c = getc(stdin);
    if (c == expect)
        return lexer_punct(b);
    ungetc(c, stdin);
    return lexer_punct(a);
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

        case '/':
            c = getc(stdin);
            switch (c) {
                case '/':
                    lexer_skip_comment_line();
                    return lexer_read_token();
                case '*':
                    lexer_skip_comment_block();
                    return lexer_read_token();
            }
            ungetc(c, stdin);
            return lexer_punct('/');

        // punctuation
        case '*':
        case '(': case ')':
        case '[': case ']':
        case '{': case '}':
        case ',': case ';':
        case '?': case ':':
        case '.': case '~':
        case '^':
            return lexer_punct(c);

        case '|': return lexer_read_reclassify('|', '|', LEXER_TOKEN_OR);
        case '&': return lexer_read_reclassify('&', '&', LEXER_TOKEN_AND);
        case '=': return lexer_read_reclassify('=', '=', LEXER_TOKEN_EQUAL);
        case '+': return lexer_read_reclassify('+', '+', LEXER_TOKEN_INCREMENT);
        case '<': return lexer_read_reclassify('=', '<', LEXER_TOKEN_LEQUAL);
        case '>': return lexer_read_reclassify('=', '>', LEXER_TOKEN_GEQUAL);
        case '!': return lexer_read_reclassify('=', '!', LEXER_TOKEN_NEQUAL);

        // special handling for ->
        case '-':
            c = getc(stdin);
            switch (c) {
                case '-': return lexer_punct(LEXER_TOKEN_DECREMENT);
                case '>': return lexer_punct(LEXER_TOKEN_ARROW);
            }
            ungetc(c, stdin);
            return lexer_punct('-');

        case EOF:
            return NULL;

        default:
            compile_error("Unexpected character: `%c`", c);
    }

    // never reached
    return NULL;
}

bool lexer_ispunct(lexer_token_t *token, int c) {
    return token && (token->type == LEXER_TOKEN_PUNCT) && (token->punct == c);
}

void lexer_unget(lexer_token_t *token) {
    if (!token)
        return;

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

bool lexer_islong(char *string) {
    // long must be all digit up to a L or l in which the last
    // character must be a null termination
    for (; *string; string++)
        if (!isdigit(*string))
            return (*string == 'L' || *string == 'l') && string[1] == '\0';
    return false;
}

bool lexer_isint(char *string) {
    // like long but none of the test
    for (; *string; string++)
        if (!isdigit(*string))
            return false;
    return true;
}

bool lexer_isfloat(char *string) {
    for (; *string; string++)
        if (!isdigit(*string))
            break;
    if (*string++ != '.')
        return false;
    for (; *string; string++)
        if (!isdigit(*string))
            return false;
    return true;
}

char *lexer_tokenstr(lexer_token_t *token) {
    string_t *string = string_create();

    if (!token)
        return "(null)";

    switch (token->type) {
        case LEXER_TOKEN_PUNCT:
            if (token->punct == LEXER_TOKEN_EQUAL) {
                string_catf(string, "==");
                return string_buffer(string);
            }
            // fall
        case LEXER_TOKEN_CHAR:
            string_cat(string, token->character);
            return string_buffer(string);

        case LEXER_TOKEN_NUMBER:
            string_catf(string, "%d", token->integer);
            return string_buffer(string);

        case LEXER_TOKEN_STRING:
            string_catf(string, "\"%s\"", token->string);
            return string_buffer(string);

        case LEXER_TOKEN_IDENT:
            return token->string;

        default:
            break;
    }
    compile_error("Internal error: unexpected token");
    return NULL;
}
