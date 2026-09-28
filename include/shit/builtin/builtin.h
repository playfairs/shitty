#ifndef SHIT_BUILTIN_BUILTIN_H
#define SHIT_BUILTIN_BUILTIN_H

#include "shit/core/shell.h"
#include "shit/parsing/parser.h"

int builtin_cd(Shell *shell, const Command *command);
int builtin_pwd(Shell *shell, const Command *command);
int builtin_exit(Shell *shell, const Command *command);
int builtin_export(Shell *shell, const Command *command);
int builtin_unset(Shell *shell, const Command *command);

#endif