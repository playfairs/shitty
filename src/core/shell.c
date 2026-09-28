#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shit/builtin/builtin.h"
#include "shit/core/shell.h"
#include "shit/execution/executor.h"
#include "shit/expansion/expansion.h"
#include "shit/input/input.h"
#include "shit/parsing/lexer.h"
#include "shit/parsing/parser.h"
#include "shit/signals/signals.h"

static int expand_alias(Shell *shell, Command *command)
{
    if (command->argc == 0)
    {
        return 0;
    }
    const char *value =
        builtin_alias_lookup(shell, command->argv[0]);
    if (value == NULL)
    {
        return 0;
    }

    TokenList tokens = {0};
    if (lexer_tokenize(value, &tokens) != 0)
    {
        return -1;
    }
    for (size_t index = 0; index < tokens.length; index++)
    {
        if (strcmp(tokens.items[index],
                   LEXER_TOKEN_SEQUENCE)
                == 0
            || strcmp(tokens.items[index], LEXER_TOKEN_PIPE)
                   == 0)
        {
            lexer_destroy(&tokens);
            errno = EINVAL;
            return -1;
        }
    }

    Command alias = {.argv = tokens.items,
                     .argc = tokens.length};
    tokens.items = NULL;
    tokens.length = 0;
    if (expansion_expand_command(&alias) != 0)
    {
        command_destroy(&alias);
        return -1;
    }

    size_t argument_count = command->argc - 1;
    if (alias.argc > (size_t)-1 - argument_count - 1)
    {
        command_destroy(&alias);
        errno = ENOMEM;
        return -1;
    }
    char **arguments =
        calloc(alias.argc + argument_count + 1,
               sizeof(*arguments));
    if (arguments == NULL)
    {
        command_destroy(&alias);
        return -1;
    }
    for (size_t index = 0; index < alias.argc; index++)
    {
        arguments[index] = alias.argv[index];
        alias.argv[index] = NULL;
    }
    for (size_t index = 0; index < argument_count; index++)
    {
        arguments[alias.argc + index] =
            command->argv[index + 1];
        command->argv[index + 1] = NULL;
    }

    size_t expanded_count = alias.argc + argument_count;
    command_destroy(&alias);
    command_destroy(command);
    command->argv = arguments;
    command->argc = expanded_count;
    return 0;
}

static void execute_line(Shell *shell, const char *line)
{
    TokenList tokens = {0};
    if (lexer_tokenize(line, &tokens) != 0)
    {
        perror("shit: lexer");
        shell->last_status = 1;
        return;
    }

    size_t position = 0;
    while (!shell->should_exit && position < tokens.length)
    {
        Command command = {0};
        int parse_result =
            parser_parse_next(&tokens, &position, &command);
        if (parse_result < 0)
        {
            perror("shit: parser");
            shell->last_status = 2;
            break;
        }
        if (parse_result == 0)
        {
            break;
        }

        if (expansion_expand_command(&command) != 0
            || expand_alias(shell, &command) != 0)
        {
            perror("shit: expansion");
            shell->last_status = 2;
            command_destroy(&command);
            continue;
        }

        shell->last_status =
            executor_execute(shell, &command);
        signals_clear_interrupt();
        command_destroy(&command);
    }
    lexer_destroy(&tokens);
}

static char *get_config_path(void)
{
    const char *override = getenv("SHITRC");
    if (override != NULL && override[0] != '\0')
    {
        return strdup(override);
    }
    const char *home = getenv("HOME");
    if (home == NULL)
    {
        return NULL;
    }
    size_t home_length = strlen(home);
    static const char filename[] = "/.shitrc";
    if (home_length > (size_t)-1 - sizeof(filename))
    {
        errno = ENOMEM;
        return NULL;
    }
    char *path = malloc(home_length + sizeof(filename));
    if (path != NULL)
    {
        memcpy(path, home, home_length);
        memcpy(path + home_length,
               filename,
               sizeof(filename));
    }
    return path;
}

static void load_config(Shell *shell)
{
    char *path = get_config_path();
    if (path == NULL)
    {
        return;
    }
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        if (errno != ENOENT)
        {
            perror("shit: config");
        }
        free(path);
        return;
    }

    char *line = NULL;
    size_t capacity = 0;
    while (!shell->should_exit
           && getline(&line, &capacity, file) != -1)
    {
        execute_line(shell, line);
    }
    free(line);
    fclose(file);
    free(path);
}

int shell_run(Shell *shell)
{
    if (shell->interactive
        && signals_initialize_shell() != 0)
    {
        perror("shit: signals");
        return 1;
    }

    char *line = NULL;
    size_t capacity = 0;
    history_initialize(&shell->history);
    prompt_initialize(&shell->prompt_config);
    shell->first_command = true;

    if (shell->interactive)
    {
        load_config(shell);
        history_load(&shell->history);
    }

    while (!shell->should_exit)
    {
        if (shell->interactive)
        {
            if (!shell->first_command)
            {
                printf("\n");
            }
            shell->first_command = false;
            char prompt_buffer[128];
            prompt_render(&shell->prompt_config,
                          prompt_buffer,
                          sizeof(prompt_buffer));
            fputs(prompt_buffer, stdout);
            fflush(stdout);
        }

        errno = 0;
        InputResult input_result =
            input_read_line(shell->interactive,
                            &shell->history,
                            &shell->prompt_config,
                            &line,
                            &capacity);
        if (input_result == INPUT_INTERRUPTED)
        {
            shell->last_status = 130;
            continue;
        }
        if (input_result == INPUT_EOF)
        {
            break;
        }
        if (input_result == INPUT_ERROR)
        {
            perror("shit: input");
            shell->last_status = 1;
            break;
        }
        if (history_add(&shell->history, line) != 0)
        {
            perror("shit: history");
        }

        execute_line(shell, line);
    }

    free(line);
    if (shell->interactive)
    {
        history_save(&shell->history);
    }
    int status = shell->should_exit ? shell->exit_status
                                    : shell->last_status;
    history_destroy(&shell->history);
    builtin_aliases_destroy(shell);
    return status;
}