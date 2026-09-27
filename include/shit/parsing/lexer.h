#ifndef SHIT_LEXER_H
#define SHIT_LEXER_H

#include <stddef.h>

typedef struct
{
    char **items;
    size_t length;
} TokenList;

int lexer_tokenize(const char *line, TokenList *tokens);
void lexer_destroy(TokenList *tokens);

#endif