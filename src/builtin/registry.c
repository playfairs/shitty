#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shit/builtin/builtin.h"
#include "shit/builtin/registry.h"
#include "shit/core/shell.h"
#include "shit/environment/environment.h"

int builtin_cd(Shell *shell, const Command *command);
int builtin_pwd(Shell *shell, const Command *command);
int builtin_exit(Shell *shell, const Command *command);

const char *builtin_alias_lookup(const Shell *shell,
                                 const char *name)
{
    for (size_t index = 0; index < shell->alias_count;
         index++)
    {
        if (strcmp(shell->aliases[index].name, name) == 0)
        {
            return shell->aliases[index].value;
        }
    }
    return NULL;
}

int builtin_alias_set(Shell *shell,
                      const char *name,
                      const char *value)
{
    if (name[0] == '\0')
    {
        return -1;
    }
    for (const char *character = name; *character != '\0';
         character++)
    {
        if (*character == '='
            || isspace((unsigned char)*character))
        {
            return -1;
        }
    }

    char *new_value = strdup(value);
    if (new_value == NULL)
    {
        return -1;
    }
    for (size_t index = 0; index < shell->alias_count;
         index++)
    {
        if (strcmp(shell->aliases[index].name, name) == 0)
        {
            free(shell->aliases[index].value);
            shell->aliases[index].value = new_value;
            return 0;
        }
    }

    if (shell->alias_count == shell->alias_capacity)
    {
        size_t capacity = shell->alias_capacity == 0
                              ? 8
                              : shell->alias_capacity * 2;
        if (capacity <= shell->alias_capacity
            || capacity
                   > (size_t)-1 / sizeof(*shell->aliases))
        {
            free(new_value);
            return -1;
        }
        ShellAlias *aliases =
            realloc(shell->aliases,
                    capacity * sizeof(*aliases));
        if (aliases == NULL)
        {
            free(new_value);
            return -1;
        }
        shell->aliases = aliases;
        shell->alias_capacity = capacity;
    }
    char *new_name = strdup(name);
    if (new_name == NULL)
    {
        free(new_value);
        return -1;
    }
    shell->aliases[shell->alias_count++] =
        (ShellAlias){.name = new_name, .value = new_value};
    return 0;
}

void builtin_aliases_destroy(Shell *shell)
{
    for (size_t index = 0; index < shell->alias_count;
         index++)
    {
        free(shell->aliases[index].name);
        free(shell->aliases[index].value);
    }
    free(shell->aliases);
    shell->aliases = NULL;
    shell->alias_count = 0;
    shell->alias_capacity = 0;
}

static int builtin_alias(Shell *shell,
                         const Command *command)
{
    if (command->argc == 1)
    {
        for (size_t index = 0; index < shell->alias_count;
             index++)
        {
            printf("alias %s='%s'\n",
                   shell->aliases[index].name,
                   shell->aliases[index].value);
        }
        return ferror(stdout) ? 1 : 0;
    }

    int status = 0;
    for (size_t index = 1; index < command->argc; index++)
    {
        const char *argument = command->argv[index];
        const char *equals = strchr(argument, '=');
        if (equals == NULL)
        {
            const char *value =
                builtin_alias_lookup(shell, argument);
            if (value == NULL)
            {
                fprintf(stderr,
                        "shit: alias: %s: not found\n",
                        argument);
                status = 1;
            }
            else
            {
                printf("alias %s='%s'\n", argument, value);
            }
            continue;
        }
        size_t name_length = (size_t)(equals - argument);
        char *name = malloc(name_length + 1);
        if (name == NULL)
        {
            return 1;
        }
        memcpy(name, argument, name_length);
        name[name_length] = '\0';
        if (builtin_alias_set(shell, name, equals + 1) != 0)
        {
            fprintf(stderr,
                    "shit: alias: invalid alias name: %s\n",
                    name);
            status = 1;
        }
        free(name);
    }
    return status;
}

static int builtin_unalias(Shell *shell,
                           const Command *command)
{
    int status = 0;
    for (size_t argument = 1; argument < command->argc;
         argument++)
    {
        size_t index = 0;
        while (index < shell->alias_count
               && strcmp(shell->aliases[index].name,
                         command->argv[argument])
                      != 0)
        {
            index++;
        }
        if (index == shell->alias_count)
        {
            fprintf(stderr,
                    "shit: unalias: %s: not found\n",
                    command->argv[argument]);
            status = 1;
            continue;
        }
        free(shell->aliases[index].name);
        free(shell->aliases[index].value);
        memmove(&shell->aliases[index],
                &shell->aliases[index + 1],
                (shell->alias_count - index - 1)
                    * sizeof(*shell->aliases));
        shell->alias_count--;
    }
    return status;
}

extern char **environ;

static int valid_name(const char *name, size_t length)
{
    if (length == 0
        || !((*name >= 'A' && *name <= 'Z')
             || (*name >= 'a' && *name <= 'z')
             || *name == '_'))
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

int builtin_export(Shell *shell, const Command *command)
{
    (void)shell;
    int status = 0;
    if (command->argc == 1)
    {
        for (char **entry = environ; *entry != NULL;
             entry++)
        {
            printf("export %s\n", *entry);
        }
        return ferror(stdout) ? 1 : 0;
    }
    for (size_t index = 1; index < command->argc; index++)
    {
        const char *argument = command->argv[index];
        const char *equals = strchr(argument, '=');
        size_t name_length =
            equals == NULL ? strlen(argument)
                           : (size_t)(equals - argument);
        if (!valid_name(argument, name_length))
        {
            fprintf(stderr,
                    "shit: export: `%s': not a valid "
                    "identifier\n",
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

int builtin_unset(Shell *shell, const Command *command)
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
                    "shit: unset: `%s': not a valid "
                    "identifier\n",
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
    {"alias", builtin_alias},
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"export", builtin_export},
    {"pwd", builtin_pwd},
    {"unalias", builtin_unalias},
    {"unset", builtin_unset}};

static const size_t builtin_count =
    sizeof(builtin_table) / sizeof(builtin_table[0]);

bool builtin_exists(const char *name)
{
    for (size_t index = 0; index < builtin_count; index++)
    {
        if (strcmp(name, builtin_table[index].name) == 0)
        {
            return true;
        }
    }
    return false;
}

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
