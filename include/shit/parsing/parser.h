#ifndef SHIT_PARSER_H
#define SHIT_PARSER_H

#include <stddef.h>

#include "shit/parsing/lexer.h"

typedef struct
{
    char **argv;
    size_t argc;
} Command;

int parser_parse(TokenList *tokens, Command *command);
int parser_parse_next(TokenList *tokens,
                      size_t *position,
                      Command *command);
void command_destroy(Command *command);

#endif