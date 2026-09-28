#ifndef SHIT_BUILTIN_BUILTIN_H
#define SHIT_BUILTIN_BUILTIN_H

#include "shit/core/shell.h"
#include "shit/parsing/parser.h"

int builtin_cd(Shell *shell, const Command *command);
int builtin_pwd(Shell *shell, const Command *command);
int builtin_exit(Shell *shell, const Command *command);
int builtin_export(Shell *shell, const Command *command);
int builtin_unset(Shell *shell, const Command *command);
const char *builtin_alias_lookup(const Shell *shell,
                                 const char *name);
int builtin_alias_set(Shell *shell,
                      const char *name,
                      const char *value);
void builtin_aliases_destroy(Shell *shell);

#endif