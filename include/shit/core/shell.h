#ifndef SHIT_SHELL_H
#define SHIT_SHELL_H

#include <stdbool.h>

#include "shit/history/history.h"
#include "shit/prompt/prompt.h"

typedef struct
{
    bool interactive;
    bool should_exit;
    int last_status;
    int exit_status;
    bool first_command;
    History history;
    PromptConfig prompt_config;
} Shell;

int shell_run(Shell *shell);

#endif