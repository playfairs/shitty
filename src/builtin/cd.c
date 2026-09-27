#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shit/builtin/builtin.h"
#include "shit/environment/environment.h"

int builtin_cd(Shell *shell, const Command *command)
{
    (void)shell;
    if (command->argc > 2)
    {
        fprintf(stderr, "shit: cd: too many arguments\n");
        return 2;
    }

    const char *directory = command->argc == 2
                                ? command->argv[1]
                                : environment_get("HOME");
    if (directory == NULL)
    {
        fprintf(stderr, "shit: cd: HOME is not set\n");
        return 1;
    }

    char *previous = getcwd(NULL, 0);
    if (chdir(directory) != 0)
    {
        perror("shit: cd");
        free(previous);
        return 1;
    }

    int status = 0;
    if (previous != NULL
        && environment_set("OLDPWD", previous) != 0)
    {
        perror("shit: cd: OLDPWD");
        status = 1;
    }
    free(previous);

    char *current = getcwd(NULL, 0);
    if (current == NULL)
    {
        perror("shit: cd: PWD");
        return 1;
    }
    if (environment_set("PWD", current) != 0)
    {
        perror("shit: cd: PWD");
        status = 1;
    }
    free(current);
    return status;
}
