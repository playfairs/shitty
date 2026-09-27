#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shit/core/shell.h"
#include "shit/execution/executor.h"
#include "shit/expansion/expansion.h"
#include "shit/input/input.h"
#include "shit/parsing/lexer.h"
#include "shit/parsing/parser.h"
#include "shit/signals/signals.h"

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

        TokenList tokens = {0};
        if (lexer_tokenize(line, &tokens) != 0)
        {
            perror("shit: lexer");
            shell->last_status = 1;
            continue;
        }

        size_t position = 0;
        while (!shell->should_exit
               && position < tokens.length)
        {
            Command command = {0};
            int parse_result = parser_parse_next(&tokens,
                                                 &position,
                                                 &command);
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

            if (expansion_expand_command(&command) != 0)
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

    free(line);
    if (shell->interactive)
    {
        history_save(&shell->history);
    }
    int status = shell->should_exit ? shell->exit_status
                                    : shell->last_status;
    history_destroy(&shell->history);
    return status;
}