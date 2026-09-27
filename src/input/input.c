#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shit/input/input.h"
#include "shit/signals/signals.h"
#include "shit/terminal/terminal.h"

static int write_all(const char *text, size_t length)
{
    size_t written = 0;
    while (written < length)
    {
        ssize_t result = write(STDOUT_FILENO,
                               text + written,
                               length - written);
        if (result < 0)
        {
            if (errno == EINTR
                && !signals_was_interrupted())
            {
                continue;
            }
            return -1;
        }
        written += (size_t)result;
    }
    return 0;
}

static int
reserve_line(char **line, size_t *capacity, size_t required)
{
    if (required <= *capacity)
    {
        return 0;
    }
    size_t next_capacity = *capacity == 0 ? 128 : *capacity;
    while (next_capacity < required)
    {
        if (next_capacity > (size_t)-1 / 2)
        {
            next_capacity = required;
            break;
        }
        next_capacity *= 2;
    }
    char *next_line = realloc(*line, next_capacity);
    if (next_line == NULL)
    {
        return -1;
    }
    *line = next_line;
    *capacity = next_capacity;
    return 0;
}

static size_t previous_character(const char *line,
                                 size_t position)
{
    if (position > 0)
    {
        position--;
        while (position > 0
               && ((unsigned char)line[position] & 0xc0)
                      == 0x80)
        {
            position--;
        }
    }
    return position;
}

static size_t next_character(const char *line,
                             size_t length,
                             size_t position)
{
    if (position < length)
    {
        position++;
        while (position < length
               && ((unsigned char)line[position] & 0xc0)
                      == 0x80)
        {
            position++;
        }
    }
    return position;
}

static int redraw_line(const char *prompt,
                       const char *line,
                       size_t length,
                       size_t cursor)
{
    if (write_all("\r\033[K", 4) != 0
        || write_all(prompt, strlen(prompt)) != 0
        || write_all(line, length) != 0)
    {
        return -1;
    }
    if (length > cursor)
    {
        char movement[32];
        size_t columns = length - cursor;
        int size = snprintf(movement,
                            sizeof(movement),
                            "\033[%zuD",
                            columns);
        if (size < 0 || (size_t)size >= sizeof(movement)
            || write_all(movement, (size_t)size) != 0)
        {
            return -1;
        }
    }
    return 0;
}

static int read_escape_key(void)
{
    struct pollfd descriptor = {.fd = STDIN_FILENO,
                                .events = POLLIN};
    if (poll(&descriptor, 1, 50) <= 0)
    {
        return 0;
    }
    char introducer;
    if (read(STDIN_FILENO, &introducer, 1) != 1
        || (introducer != '[' && introducer != 'O'))
    {
        return 0;
    }
    for (size_t length = 0; length < 8; length++)
    {
        if (poll(&descriptor, 1, 50) <= 0)
        {
            return 0;
        }
        char character;
        if (read(STDIN_FILENO, &character, 1) != 1)
        {
            return 0;
        }
        if (character >= '@' && character <= '~')
        {
            return character;
        }
    }
    return 0;
}

static int fzf_available(void)
{
    static int checked = 0;
    static int available = 0;
    if (!checked)
    {
        checked = 1;
        available =
            system("command -v fzf >/dev/null 2>&1") == 0;
    }
    return available;
}

static int
launch_fzf(History *history, char **line, size_t *capacity)
{
    if (!fzf_available())
    {
        fprintf(stderr,
                "\r\033[Kshit: fzf not available\n");
        return -1;
    }

    if (history_get_count(history) == 0)
    {
        return -1;
    }

    char temp_file[] = "/tmp/shit_history_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1)
    {
        return -1;
    }
    close(fd);

    FILE *file = fopen(temp_file, "w");
    if (file == NULL)
    {
        unlink(temp_file);
        return -1;
    }

    for (size_t index = 0;
         index < history_get_count(history);
         index++)
    {
        fprintf(file,
                "%s\n",
                history_get_entry(history, index));
    }
    fclose(file);

    char command[512];
    snprintf(command,
             sizeof(command),
             "fzf < %s",
             temp_file);

    FILE *fzf = popen(command, "r");
    if (fzf == NULL)
    {
        unlink(temp_file);
        return -1;
    }

    char *selected = NULL;
    size_t selected_capacity = 0;
    ssize_t length =
        getline(&selected, &selected_capacity, fzf);
    pclose(fzf);
    unlink(temp_file);

    if (length > 0 && selected[length - 1] == '\n')
    {
        selected[length - 1] = '\0';
    }

    if (selected != NULL && selected[0] != '\0')
    {
        if (reserve_line(line,
                         capacity,
                         strlen(selected) + 1)
            == 0)
        {
            strcpy(*line, selected);
        }
    }
    free(selected);
    return 0;
}

static InputResult finish_input(struct termios *original,
                                int terminal_active,
                                InputResult result)
{
    int restore_result =
        terminal_active ? terminal_end_input(original) : 0;
    if (result == INPUT_LINE || result == INPUT_EOF
        || result == INPUT_INTERRUPTED)
    {
        write_all("\n", 1);
    }
    if (restore_result != 0)
    {
        return INPUT_ERROR;
    }
    return result;
}

static InputResult
read_interactive_line(History *history,
                      const PromptConfig *prompt_config,
                      char **line,
                      size_t *capacity)
{
    struct termios original;
    if (terminal_begin_input(&original) != 0)
    {
        return INPUT_ERROR;
    }
    if (reserve_line(line, capacity, 1) != 0)
    {
        terminal_end_input(&original);
        return INPUT_ERROR;
    }

    char prompt_buffer[128];
    prompt_render(prompt_config,
                  prompt_buffer,
                  sizeof(prompt_buffer));

    size_t length = 0;
    size_t cursor = 0;
    (*line)[0] = '\0';
    while (1)
    {
        if (signals_was_interrupted())
        {
            (*line)[0] = '\0';
            history_reset_navigation(history);
            signals_clear_interrupt();
            return finish_input(&original,
                                1,
                                INPUT_INTERRUPTED);
        }

        char character;
        ssize_t count = read(STDIN_FILENO, &character, 1);
        if (count < 0)
        {
            if (errno == EINTR)
            {
                if (signals_was_interrupted())
                {
                    (*line)[0] = '\0';
                    history_reset_navigation(history);
                    signals_clear_interrupt();
                    return finish_input(&original,
                                        1,
                                        INPUT_INTERRUPTED);
                }
                continue;
            }
            history_reset_navigation(history);
            return finish_input(&original, 1, INPUT_ERROR);
        }
        if (count == 0)
        {
            history_reset_navigation(history);
            return finish_input(&original, 1, INPUT_EOF);
        }

        if (character == '\n' || character == '\r'
            || character == 4)
        {
            if (character == 4 && length == 0)
            {
                history_reset_navigation(history);
                return finish_input(&original,
                                    1,
                                    INPUT_EOF);
            }
            (*line)[length] = '\0';
            history_reset_navigation(history);
            return finish_input(&original, 1, INPUT_LINE);
        }

        if (character == 3)
        {
            (*line)[0] = '\0';
            history_reset_navigation(history);
            return finish_input(&original,
                                1,
                                INPUT_INTERRUPTED);
        }

        if (character == 6)
        {
            terminal_end_input(&original);
            launch_fzf(history, line, capacity);
            terminal_begin_input(&original);
            length = strlen(*line);
            cursor = length;
            redraw_line(prompt_buffer,
                        *line,
                        length,
                        cursor);
            continue;
        }

        if (character == '\033')
        {
            int key = read_escape_key();
            const char *recalled = NULL;
            if (key == 'A')
            {
                recalled = history_previous(history, *line);
            }
            else if (key == 'B')
            {
                recalled = history_next(history);
            }
            else if (key == 'C')
            {
                cursor =
                    next_character(*line, length, cursor);
            }
            else if (key == 'D')
            {
                cursor = previous_character(*line, cursor);
            }
            if (recalled != NULL)
            {
                size_t recalled_length = strlen(recalled);
                if (reserve_line(line,
                                 capacity,
                                 recalled_length + 1)
                    != 0)
                {
                    history_reset_navigation(history);
                    return finish_input(&original,
                                        1,
                                        INPUT_ERROR);
                }
                memcpy(*line,
                       recalled,
                       recalled_length + 1);
                length = recalled_length;
                cursor = length;
            }
            if (redraw_line(prompt_buffer,
                            *line,
                            length,
                            cursor)
                != 0)
            {
                history_reset_navigation(history);
                return finish_input(&original,
                                    1,
                                    INPUT_ERROR);
            }
            continue;
        }

        if (character == 127 || character == '\b')
        {
            if (cursor > 0)
            {
                size_t previous =
                    previous_character(*line, cursor);
                memmove(*line + previous,
                        *line + cursor,
                        length - cursor + 1);
                length -= cursor - previous;
                cursor = previous;
                if (redraw_line(prompt_buffer,
                                *line,
                                length,
                                cursor)
                    != 0)
                {
                    return finish_input(&original,
                                        1,
                                        INPUT_ERROR);
                }
            }
            continue;
        }

        if ((unsigned char)character >= 32
            && character != 127)
        {
            if (reserve_line(line, capacity, length + 2)
                != 0)
            {
                history_reset_navigation(history);
                return finish_input(&original,
                                    1,
                                    INPUT_ERROR);
            }
            memmove(*line + cursor + 1,
                    *line + cursor,
                    length - cursor + 1);
            (*line)[cursor++] = character;
            length++;
            if (redraw_line(prompt_buffer,
                            *line,
                            length,
                            cursor)
                != 0)
            {
                history_reset_navigation(history);
                return finish_input(&original,
                                    1,
                                    INPUT_ERROR);
            }
        }
    }
}

InputResult
input_read_line(bool interactive,
                History *history,
                const PromptConfig *prompt_config,
                char **line,
                size_t *capacity)
{
    if (!interactive)
    {
        return getline(line, capacity, stdin) < 0
                   ? (feof(stdin) ? INPUT_EOF : INPUT_ERROR)
                   : INPUT_LINE;
    }
    return read_interactive_line(history,
                                 prompt_config,
                                 line,
                                 capacity);
}