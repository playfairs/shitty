#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "shit/builtin/builtin.h"

int builtin_pwd(Shell *shell, const Command *command)
{
    (void)shell;
    if (command->argc != 1)
    {
        fprintf(stderr, "shit: pwd: unexpected argument\n");
        return 2;
    }

    char *directory = getcwd(NULL, 0);
    if (directory == NULL)
    {
        perror("shit: pwd");
        return 1;
    }
    puts(directory);
    free(directory);
    return 0;
}
