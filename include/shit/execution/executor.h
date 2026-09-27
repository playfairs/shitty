#ifndef SHIT_EXECUTOR_H
#define SHIT_EXECUTOR_H

#include "shit/core/shell.h"
#include "shit/parsing/parser.h"

int executor_execute(Shell *shell, const Command *command);

#endif