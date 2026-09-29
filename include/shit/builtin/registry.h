#ifndef SHIT_BUILTIN_REGISTRY_H
#define SHIT_BUILTIN_REGISTRY_H

#include <stdbool.h>

#include "shit/core/shell.h"
#include "shit/parsing/parser.h"

typedef int (*BuiltinFunction)(Shell *shell,
                               const Command *command);

typedef struct
{
    const char *name;
    BuiltinFunction function;
} BuiltinEntry;

bool builtin_execute(Shell *shell,
                     const Command *command,
                     int *status);
bool builtin_exists(const char *name);

#endif
