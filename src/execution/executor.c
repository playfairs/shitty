#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shit/builtin/registry.h"
#include "shit/execution/executor.h"
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

int executor_execute(Shell *shell, const Command *command)
{
    if (command->argc == 0)
    {
        return shell->last_status;
    }

    int status = 0;
    if (builtin_execute(shell, command, &status))
    {
        return status;
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
        execvp(command->argv[0], command->argv);
        int error = errno;
        if (error == ENOENT
            && strchr(command->argv[0], '/') == NULL)
        {
            fprintf(stderr,
                    "shit: command not found: %s\n",
                    command->argv[0]);
        }
        else if (strchr(command->argv[0], '/') != NULL)
        {
            fprintf(stderr,
                    "\"%s\": %s (os error %d)\n",
                    command->argv[0],
                    strerror(error),
                    error);
        }
        else
        {
            fprintf(stderr,
                    "shit: %s: %s\n",
                    command->argv[0],
                    strerror(error));
        }
        _exit(error == ENOENT ? 127 : 126);
    }

    int child_status = 0;
    while (waitpid(child, &child_status, 0) < 0)
    {
        if (errno == EINTR)
        {
            continue;
        }
        perror("shit: waitpid");
        return 1;
    }
    return process_status(child_status);
}