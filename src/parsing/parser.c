#include <stdlib.h>
#include <string.h>

#include "shit/parsing/parser.h"

int parser_parse(TokenList *tokens, Command *command)
{
    command->argv = tokens->items;
    command->argc = tokens->length;
    tokens->items = NULL;
    tokens->length = 0;
    return 0;
}

int parser_parse_next(TokenList *tokens,
                      size_t *position,
                      Command *command)
{
    command->argv = NULL;
    command->argc = 0;

    size_t start = *position;
    while (start < tokens->length
           && strcmp(tokens->items[start], ";") == 0)
    {
        start++;
    }
    if (start == tokens->length)
    {
        *position = start;
        return 0;
    }

    size_t end = start;
    while (end < tokens->length
           && strcmp(tokens->items[end], ";") != 0)
    {
        end++;
    }

    size_t argc = end - start;
    char **argv = calloc(argc + 1, sizeof(*argv));
    if (argv == NULL)
    {
        return -1;
    }
    for (size_t index = 0; index < argc; index++)
    {
        argv[index] = tokens->items[start + index];
        tokens->items[start + index] = NULL;
    }

    command->argv = argv;
    command->argc = argc;
    *position = end < tokens->length ? end + 1 : end;
    return 1;
}

void command_destroy(Command *command)
{
    for (size_t index = 0; index < command->argc; index++)
    {
        free(command->argv[index]);
    }
    free(command->argv);
    command->argv = NULL;
    command->argc = 0;
}