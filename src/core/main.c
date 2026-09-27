#include <stdio.h>
#include <unistd.h>

#include "shit/core/shell.h"

int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
    {
        fprintf(stderr,
                "shit: command-line arguments are not "
                "supported yet\n");
        return 2;
    }

    Shell shell = {.interactive = isatty(STDIN_FILENO) != 0,
                   .should_exit = false,
                   .last_status = 0,
                   .exit_status = 0,
                   .first_command = true};
    return shell_run(&shell);
}