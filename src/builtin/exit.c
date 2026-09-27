#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shit/builtin/builtin.h"
#include "shit/core/shell.h"

int builtin_exit(Shell *shell, const Command *command)
{
    if (command->argc > 2)
    {
        fprintf(stderr, "shit: exit: too many arguments\n");
        return 2;
    }

    int status = shell->last_status;
    if (command->argc == 2)
    {
        errno = 0;
        char *end = NULL;
        long value = strtol(command->argv[1], &end, 10);
        if (errno == ERANGE || end == command->argv[1]
            || *end != '\0')
        {
            fprintf(stderr,
                    "shit: exit: numeric argument "
                    "required: %s\n",
                    command->argv[1]);
            shell->should_exit = true;
            shell->exit_status = 2;
            return 2;
        }
        status = (unsigned char)value;
    }

    shell->should_exit = true;
    shell->exit_status = status;
    return status;
}
