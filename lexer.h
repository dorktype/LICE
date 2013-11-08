#ifndef LICE_LEXER_HDR
#define LICE_LEXER_HDR
#include <stdbool.h>

typedef enum {
    LEXER_TOKEN_IDENTIFIER,
    LEXER_TOKEN_PUNCT,
    LEXER_TOKEN_CHAR,
    LEXER_TOKEN_STRING,
    LEXER_TOKEN_NUMBER,

    // reclassified tokens need to not conflict with
    // the ast types
    LEXER_TOKEN_EQUAL     = 0x200,
    LEXER_TOKEN_LEQUAL,
    LEXER_TOKEN_GEQUAL,
    LEXER_TOKEN_NEQUAL,
    LEXER_TOKEN_INCREMENT,
    LEXER_TOKEN_DECREMENT,
    LEXER_TOKEN_ARROW,

    LEXER_TOKEN_LSHIFT,
    LEXER_TOKEN_RSHIFT,

    // compound assignments
    LEXER_TOKEN_COMPOUND_ADD,
    LEXER_TOKEN_COMPOUND_SUB,
    LEXER_TOKEN_COMPOUND_MUL,
    LEXER_TOKEN_COMPOUND_DIV,
    LEXER_TOKEN_COMPOUND_MOD,
    LEXER_TOKEN_COMPOUND_AND,
    LEXER_TOKEN_COMPOUND_OR,
    LEXER_TOKEN_COMPOUND_XOR,
    LEXER_TOKEN_COMPOUND_LSHIFT,
    LEXER_TOKEN_COMPOUND_RSHIFT,

    // logical, not bitwise
    LEXER_TOKEN_AND,
    LEXER_TOKEN_OR
} lexer_token_type_t;

typedef struct {
    lexer_token_type_t type;
    union {
        long  integer;
        int   punct;
        char *string;
        char  character;
    };
} lexer_token_t;

bool lexer_islong(char *string);
bool lexer_isint(char *string);
bool lexer_isfloat(char *string);
bool lexer_ispunct(lexer_token_t *token, int c);
void lexer_unget(lexer_token_t *token);
lexer_token_t *lexer_next(void);
lexer_token_t *lexer_peek(void);

char *lexer_tokenstr(lexer_token_t *token);

#endif
