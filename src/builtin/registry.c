#include <string.h>

#include "shit/builtin/registry.h"
#include "shit/core/shell.h"

int builtin_cd(Shell *shell, const Command *command);
int builtin_pwd(Shell *shell, const Command *command);
int builtin_exit(Shell *shell, const Command *command);

static const BuiltinEntry builtin_table[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"pwd", builtin_pwd}};

static const size_t builtin_count =
    sizeof(builtin_table) / sizeof(builtin_table[0]);

bool builtin_execute(Shell *shell,
                     const Command *command,
                     int *status)
{
    if (command->argc == 0)
    {
        return false;
    }
    for (size_t index = 0; index < builtin_count; index++)
    {
        if (strcmp(command->argv[0],
                   builtin_table[index].name)
            == 0)
        {
            *status =
                builtin_table[index].function(shell,
                                              command);
            return true;
        }
    }
    return false;
}
