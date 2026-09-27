#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "shit/parsing/lexer.h"

static int reserve_items(TokenList *tokens, size_t count)
{
    if (count > (size_t)-1 / sizeof(*tokens->items))
    {
        errno = ENOMEM;
        return -1;
    }
    char **items =
        realloc(tokens->items, count * sizeof(*items));
    if (items == NULL)
    {
        return -1;
    }
    tokens->items = items;
    return 0;
}

int lexer_tokenize(const char *line, TokenList *tokens)
{
    tokens->items = NULL;
    tokens->length = 0;
    size_t capacity = 0;
    size_t position = 0;

    while (line[position] != '\0')
    {
        while (isspace((unsigned char)line[position]))
        {
            position++;
        }
        if (line[position] == '\0')
        {
            break;
        }

        size_t start = position;
        if (line[position] == ';')
        {
            position++;
        }
        else
        {
            while (
                line[position] != '\0'
                && line[position] != ';'
                && !isspace((unsigned char)line[position]))
            {
                position++;
            }
        }
        size_t length = position - start;
        if (tokens->length + 1 >= capacity)
        {
            size_t next_capacity =
                capacity == 0 ? 8 : capacity * 2;
            if (next_capacity <= capacity
                || reserve_items(tokens, next_capacity)
                       != 0)
            {
                lexer_destroy(tokens);
                return -1;
            }
            capacity = next_capacity;
        }

        char *word = malloc(length + 1);
        if (word == NULL)
        {
            lexer_destroy(tokens);
            return -1;
        }
        memcpy(word, line + start, length);
        word[length] = '\0';
        tokens->items[tokens->length++] = word;
    }

    if (tokens->length + 1 > capacity
        && reserve_items(tokens, tokens->length + 1) != 0)
    {
        lexer_destroy(tokens);
        return -1;
    }
    tokens->items[tokens->length] = NULL;
    return 0;
}

void lexer_destroy(TokenList *tokens)
{
    for (size_t index = 0; index < tokens->length; index++)
    {
        free(tokens->items[index]);
    }
    free(tokens->items);
    tokens->items = NULL;
    tokens->length = 0;
}