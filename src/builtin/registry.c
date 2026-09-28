#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shit/builtin/registry.h"
#include "shit/core/shell.h"
#include "shit/environment/environment.h"

int builtin_cd(Shell *shell, const Command *command);
int builtin_pwd(Shell *shell, const Command *command);
int builtin_exit(Shell *shell, const Command *command);

extern char **environ;

static int valid_name(const char *name, size_t length)
{
    if (length == 0
        || !((*name >= 'A' && *name <= 'Z')
             || (*name >= 'a' && *name <= 'z') || *name == '_'))
    {
        return 0;
    }
    for (size_t index = 1; index < length; index++)
    {
        char character = name[index];
        if (!((character >= 'A' && character <= 'Z')
              || (character >= 'a' && character <= 'z')
              || (character >= '0' && character <= '9')
              || character == '_'))
        {
            return 0;
        }
    }
    return 1;
}

static int builtin_export(Shell *shell, const Command *command)
{
    (void)shell;
    int status = 0;
    if (command->argc == 1)
    {
        for (char **entry = environ; *entry != NULL; entry++)
        {
            printf("export %s\n", *entry);
        }
        return ferror(stdout) ? 1 : 0;
    }
    for (size_t index = 1; index < command->argc; index++)
    {
        const char *argument = command->argv[index];
        const char *equals = strchr(argument, '=');
        size_t name_length = equals == NULL
                                 ? strlen(argument)
                                 : (size_t)(equals - argument);
        if (!valid_name(argument, name_length))
        {
            fprintf(stderr,
                    "shit: export: `%s': not a valid identifier\n",
                    argument);
            status = 1;
            continue;
        }
        char *name = malloc(name_length + 1);
        if (name == NULL)
        {
            return 1;
        }
        memcpy(name, argument, name_length);
        name[name_length] = '\0';
        if (equals != NULL)
        {
            if (environment_set(name, equals + 1) != 0)
            {
                status = 1;
            }
        }
        else if (getenv(name) == NULL
                 && environment_set(name, "") != 0)
        {
            status = 1;
        }
        free(name);
    }
    return status;
}

static int builtin_unset(Shell *shell, const Command *command)
{
    (void)shell;
    int status = 0;
    for (size_t index = 1; index < command->argc; index++)
    {
        const char *name = command->argv[index];
        size_t length = strlen(name);
        if (!valid_name(name, length))
        {
            fprintf(stderr,
                    "shit: unset: `%s': not a valid identifier\n",
                    name);
            status = 1;
        }
        else if (unsetenv(name) != 0)
        {
            status = 1;
        }
    }
    return status;
}

static const BuiltinEntry builtin_table[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"export", builtin_export},
    {"pwd", builtin_pwd},
    {"unset", builtin_unset}};

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
