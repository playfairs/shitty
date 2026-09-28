#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shit/builtin/registry.h"
#include "shit/environment/environment.h"
#include "shit/execution/executor.h"
#include "shit/parsing/lexer.h"
#include "shit/signals/signals.h"

static int process_status(int status)
{
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static size_t assignment_prefix(char *const *argv, size_t argc)
{
    size_t count = 0;
    while (count < argc)
    {
        const char *word = argv[count];
        const char *equals = strchr(word, '=');
        if (equals == NULL || equals == word
            || !((*word >= 'A' && *word <= 'Z')
                 || (*word >= 'a' && *word <= 'z')
                 || *word == '_'))
        {
            break;
        }
        int valid = 1;
        for (const char *character = word + 1;
             character < equals;
             character++)
        {
            if (!((*character >= 'A' && *character <= 'Z')
                  || (*character >= 'a' && *character <= 'z')
                  || (*character >= '0' && *character <= '9')
                  || *character == '_'))
            {
                valid = 0;
                break;
            }
        }
        if (!valid)
        {
            break;
        }
        count++;
    }
    return count;
}

static int apply_assignments(char *const *argv, size_t count)
{
    for (size_t index = 0; index < count; index++)
    {
        const char *equals = strchr(argv[index], '=');
        size_t name_length = (size_t)(equals - argv[index]);
        char *name = malloc(name_length + 1);
        if (name == NULL)
        {
            return -1;
        }
        memcpy(name, argv[index], name_length);
        name[name_length] = '\0';
        int result = environment_set(name, equals + 1);
        free(name);
        if (result != 0)
        {
            return -1;
        }
    }
    return 0;
}

static int run_child_command(Shell *shell,
                             char *const *argv,
                             size_t argc)
{
    size_t assignments = assignment_prefix(argv, argc);
    if (apply_assignments(argv, assignments) != 0)
    {
        perror("shit: assignment");
        return 1;
    }

    if (assignments == argc)
    {
        return 0;
    }

    Command command = {.argv = (char **)argv + assignments,
                       .argc = argc - assignments};
    int status = 0;
    if (builtin_execute(shell, &command, &status))
    {
        return status;
    }

    execvp(command.argv[0], command.argv);
    int error = errno;
    if (error == ENOENT && strchr(command.argv[0], '/') == NULL)
    {
        fprintf(stderr,
                "shit: command not found: %s\n",
                command.argv[0]);
    }
    else if (strchr(command.argv[0], '/') != NULL)
    {
        fprintf(stderr,
                "\"%s\": %s (os error %d)\n",
                command.argv[0],
                strerror(error),
                error);
    }
    else
    {
        fprintf(stderr,
                "shit: %s: %s\n",
                command.argv[0],
                strerror(error));
    }
    return error == ENOENT ? 127 : 126;
}

static int wait_for_child(pid_t child, int *status)
{
    while (waitpid(child, status, 0) < 0)
    {
        if (errno != EINTR)
        {
            return -1;
        }
    }
    return 0;
}

static int execute_single(Shell *shell, const Command *command)
{
    size_t assignments = assignment_prefix(command->argv,
                                           command->argc);
    if (assignments == command->argc)
    {
        if (apply_assignments(command->argv, assignments) != 0)
        {
            perror("shit: assignment");
            return 1;
        }
        return 0;
    }
    if (assignments == 0)
    {
        int status = 0;
        if (builtin_execute(shell, command, &status))
        {
            return status;
        }
    }

    fflush(NULL);
    pid_t child = fork();
    if (child < 0)
    {
        perror("shit: fork");
        return 1;
    }
    if (child == 0)
    {
        signals_prepare_child();
        int status = run_child_command(shell,
                                       command->argv,
                                       command->argc);
        fflush(NULL);
        _exit(status);
    }
    int child_status = 0;
    if (wait_for_child(child, &child_status) != 0)
    {
        perror("shit: waitpid");
        return 1;
    }
    return process_status(child_status);
}

static int execute_pipeline(Shell *shell, const Command *command)
{
    size_t stages = 1;
    for (size_t index = 0; index < command->argc; index++)
    {
        if (strcmp(command->argv[index], LEXER_TOKEN_PIPE) == 0)
        {
            stages++;
        }
    }

    pid_t *children = calloc(stages, sizeof(*children));
    if (children == NULL)
    {
        perror("shit: pipeline");
        return 1;
    }

    size_t start = 0;
    size_t child_count = 0;
    int previous_read = -1;
    int failed = 0;
    while (start < command->argc)
    {
        size_t end = start;
        while (end < command->argc
               && strcmp(command->argv[end], LEXER_TOKEN_PIPE) != 0)
        {
            end++;
        }
        if (end == start)
        {
            fprintf(stderr,
                    "shit: syntax error near unexpected token `|'\n");
            failed = 1;
            break;
        }

        int descriptors[2] = {-1, -1};
        int is_last = child_count + 1 == stages;
        if (!is_last && pipe(descriptors) != 0)
        {
            perror("shit: pipe");
            failed = 1;
            break;
        }

        fflush(NULL);
        pid_t child = fork();
        if (child < 0)
        {
            perror("shit: fork");
            if (descriptors[0] >= 0)
            {
                close(descriptors[0]);
                close(descriptors[1]);
            }
            failed = 1;
            break;
        }
        if (child == 0)
        {
            signals_prepare_child();
            if ((previous_read >= 0
                 && dup2(previous_read, STDIN_FILENO) < 0)
                || (descriptors[1] >= 0
                    && dup2(descriptors[1], STDOUT_FILENO) < 0))
            {
                perror("shit: dup2");
                _exit(1);
            }
            if (previous_read >= 0)
            {
                close(previous_read);
            }
            if (descriptors[0] >= 0)
            {
                close(descriptors[0]);
                close(descriptors[1]);
            }

            size_t argc = end - start;
            char **argv = calloc(argc + 1, sizeof(*argv));
            if (argv == NULL)
            {
                perror("shit: pipeline");
                _exit(1);
            }
            for (size_t index = 0; index < argc; index++)
            {
                argv[index] = command->argv[start + index];
            }
            int status = run_child_command(shell, argv, argc);
            free(argv);
            fflush(NULL);
            _exit(status);
        }

        children[child_count++] = child;
        if (previous_read >= 0)
        {
            close(previous_read);
        }
        if (descriptors[1] >= 0)
        {
            close(descriptors[1]);
        }
        previous_read = descriptors[0];
        start = end + 1;
        if (is_last)
        {
            start = command->argc;
        }
    }

    if (previous_read >= 0)
    {
        close(previous_read);
    }

    int last_status = 1;
    for (size_t index = 0; index < child_count; index++)
    {
        int child_status = 0;
        if (wait_for_child(children[index], &child_status) != 0)
        {
            perror("shit: waitpid");
            failed = 1;
        }
        if (index + 1 == child_count)
        {
            last_status = process_status(child_status);
        }
    }
    free(children);
    return failed ? 2 : last_status;
}

int executor_execute(Shell *shell, const Command *command)
{
    if (command->argc == 0)
    {
        return shell->last_status;
    }

    for (size_t index = 0; index < command->argc; index++)
    {
        if (strcmp(command->argv[index], LEXER_TOKEN_PIPE) == 0)
        {
            if (index == 0
                || index + 1 == command->argc
                || strcmp(command->argv[index + 1],
                          LEXER_TOKEN_PIPE)
                       == 0)
            {
                fprintf(stderr,
                        "shit: syntax error near unexpected token `|'\n");
                return 2;
            }
            return execute_pipeline(shell, command);
        }
    }
    return execute_single(shell, command);
}