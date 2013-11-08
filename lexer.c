#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "util.h"
#include "lice.h"

static list_t *lexer_buffer = &SENTINEL_LIST;

static lexer_token_t *lexer_token_copy(lexer_token_t *token) {
    return memcpy(malloc(sizeof(lexer_token_t)), token, sizeof(lexer_token_t));
}

static lexer_token_t *lexer_identifier(string_t *str) {
    return lexer_token_copy(&(lexer_token_t){
        .type      = LEXER_TOKEN_IDENTIFIER,
        .string    = string_buffer(str)
    });
}
static lexer_token_t *lexer_strtok(string_t *str) {
    return lexer_token_copy(&(lexer_token_t){
        .type      = LEXER_TOKEN_STRING,
        .string    = string_buffer(str)
    });
}
static lexer_token_t *lexer_punct(int punct) {
    return lexer_token_copy(&(lexer_token_t){
        .type      = LEXER_TOKEN_PUNCT,
        .punct     = punct
    });
}
static lexer_token_t *lexer_number(char *string) {
    return lexer_token_copy(&(lexer_token_t){
        .type      = LEXER_TOKEN_NUMBER,
        .string    = string
    });
}
static lexer_token_t *lexer_char(char value) {
    return lexer_token_copy(&(lexer_token_t){
        .type      = LEXER_TOKEN_CHAR,
        .character = value
    });
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

static bool lexer_read_character_octal_brace(int c, int *r) {
    if ('0' <= c && c <= '7') {
        *r = (*r << 3) | (c - '0');
        return true;
    }
    return false;
}

static int lexer_read_character_octal(int c) {
    int r = c - '0';
    if (lexer_read_character_octal_brace((c = getc(stdin)), &r)) {
        if (!lexer_read_character_octal_brace((c = getc(stdin)), &r))
            ungetc(c, stdin);
    } else
        ungetc(c, stdin);
    return r;
}

static int lexer_read_character_hexadecimal(void) {
    int c = getc(stdin);
    int r = 0;

    if (!isxdigit(c))
        compile_error("malformatted hexadecimal character");

    for (;; c = getc(stdin)) {
        switch (c) {
            case '0' ... '9': r = (r << 4) | (c - '0');      continue;
            case 'a' ... 'f': r = (r << 4) | (c - 'a' + 10); continue;
            case 'A' ... 'F': r = (r << 4) | (c - 'f' + 10); continue;

            default:
                ungetc(c, stdin);
                return r;
        }
    }
    return -1;
}

static int lexer_read_character_escaped(void) {
    int c = getc(stdin);

    switch (c) {
        case '\'':        return '\'';
        case '"':         return '"';
        case '?':         return '?';
        case '\\':        return '\\';
        case 'a':         return '\a';
        case 'b':         return '\b';
        case 'f':         return '\f';
        case 'n':         return '\n';
        case 'r':         return '\r';
        case 't':         return '\t';
        case 'v':         return '\v';
        case 'e':         return '\033';
        case '0' ... '7': return lexer_read_character_octal(c);
        case 'x':         return lexer_read_character_hexadecimal();
        case EOF:
            compile_error("malformatted escape sequence");

        default:
            return c;
    }
}

static lexer_token_t *lexer_read_character(void) {
    int c = getc(stdin);
    int r = (c == '\\') ? lexer_read_character_escaped() : c;

    if (getc(stdin) != '\'')
        compile_error("unterminated character");

    return lexer_char((char)r);
}

static lexer_token_t *lexer_read_string(void) {
    string_t *string = string_create();
    for (;;) {
        int c = getc(stdin);
        if (c == EOF)
            compile_error("Expected termination for string literal");

        if (c == '"')
            break;

        /* TODO fix */
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
        string_cat(string, c);
    }
    return lexer_strtok(string);
}

static lexer_token_t *lexer_read_identifier(int c1) {
    string_t *string = string_create();
    string_cat(string, (char)c1);

    for (;;) {
        int c2 = getc(stdin);
        if (isalnum(c2) || c2 == '_' || c2 == '$') {
            string_cat(string, c2);
        } else {
            ungetc(c2, stdin);
            return lexer_identifier(string);
        }
    }
    return NULL;
}

static lexer_token_t *lexer_read_reclassify_one(int expect1, int a, int e) {
    int c = getc(stdin);
    if (c == expect1) return lexer_punct(a);
    ungetc(c, stdin);
    return lexer_punct(e);
}
static lexer_token_t *lexer_read_reclassify_two(int expect1, int a, int expect2, int b, int e) {
    int c = getc(stdin);
    if (c == expect1) return lexer_punct(a);
    if (c == expect2) return lexer_punct(b);
    ungetc(c, stdin);
    return lexer_punct(e);
}

static lexer_token_t *lexer_read_token(void) {
    int c;
    lexer_skip();

    switch ((c = getc(stdin))) {
        case '0' ... '9':  return lexer_read_number(c);
        case '"':          return lexer_read_string();
        case '\'':         return lexer_read_character();
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
            if (c == '=')
                return lexer_punct(LEXER_TOKEN_COMPOUND_DIV);
            ungetc(c, stdin);
            return lexer_punct('/');

        case '(': case ')':
        case ',': case ';':
        case '[': case ']':
        case '{': case '}':
        case '?': case ':':
        case '~':
            return lexer_punct(c);

        case '+': return lexer_read_reclassify_two('+', LEXER_TOKEN_INCREMENT,    '=', LEXER_TOKEN_COMPOUND_ADD, '+');
        case '&': return lexer_read_reclassify_two('&', LEXER_TOKEN_AND,          '=', LEXER_TOKEN_COMPOUND_AND, '&');
        case '|': return lexer_read_reclassify_two('|', LEXER_TOKEN_OR,           '=', LEXER_TOKEN_COMPOUND_OR,  '|');
        case '*': return lexer_read_reclassify_one('=', LEXER_TOKEN_COMPOUND_MUL, '*');
        case '%': return lexer_read_reclassify_one('=', LEXER_TOKEN_COMPOUND_MOD, '%');
        case '=': return lexer_read_reclassify_one('=', LEXER_TOKEN_EQUAL,        '=');
        case '!': return lexer_read_reclassify_one('=', LEXER_TOKEN_NEQUAL,       '!');
        case '^': return lexer_read_reclassify_one('=', LEXER_TOKEN_COMPOUND_XOR, '^');

        case '-':
            switch ((c = getc(stdin))) {
                case '-': return lexer_punct(LEXER_TOKEN_DECREMENT);
                case '>': return lexer_punct(LEXER_TOKEN_ARROW);
                case '=': return lexer_punct(LEXER_TOKEN_COMPOUND_SUB);
                default:
                    break;
            }
            ungetc(c, stdin);
            return lexer_punct('-');

        case '<':
            if ((c = getc(stdin)) == '=')
                return lexer_punct(LEXER_TOKEN_LEQUAL);
            if (c == '<')
                return lexer_read_reclassify_one('=', LEXER_TOKEN_COMPOUND_LSHIFT, LEXER_TOKEN_LSHIFT);
            ungetc(c, stdin);
            return lexer_punct('<');
        case '>':
            if ((c = getc(stdin)) == '=')
                return lexer_punct(LEXER_TOKEN_GEQUAL);
            if (c == '>')
                return lexer_read_reclassify_one('=', LEXER_TOKEN_COMPOUND_RSHIFT, LEXER_TOKEN_RSHIFT);
            ungetc(c, stdin);
            return lexer_punct('>');

        case '.':
            c = getc(stdin);
            if (c == '.') {
                string_t *str = string_create();
                string_catf(str, "..%c", getc(stdin));
                return lexer_identifier(str);
            }
            ungetc(c, stdin);
            return lexer_punct('.');

        case EOF:
            return NULL;

        default:
            compile_error("Unexpected character: `%c`", c);
    }
    return NULL;
}

bool lexer_ispunct(lexer_token_t *token, int c) {
    return token && (token->type == LEXER_TOKEN_PUNCT) && (token->punct == c);
}

void lexer_unget(lexer_token_t *token) {
    if (!token)
        return;
    list_push(lexer_buffer, token);
}

lexer_token_t *lexer_next(void) {
    if (list_length(lexer_buffer) > 0)
        return list_pop(lexer_buffer);
    return lexer_read_token();
}

lexer_token_t *lexer_peek(void) {
    lexer_token_t *token = lexer_next();
    lexer_unget(token);
    return token;
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
        case LEXER_TOKEN_CHAR:
            string_cat(string, token->character);
            return string_buffer(string);
        case LEXER_TOKEN_NUMBER:
            string_catf(string, "%d", token->integer);
            return string_buffer(string);
        case LEXER_TOKEN_STRING:
            string_catf(string, "\"%s\"", token->string);
            return string_buffer(string);
        case LEXER_TOKEN_IDENTIFIER:
            return token->string;
        default:
            break;
    }
    compile_error("Internal error: unexpected token");
    return NULL;
}
