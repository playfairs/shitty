#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shit/input/input.h"
#include "shit/signals/signals.h"
#include "shit/terminal/terminal.h"

typedef struct
{
    char **items;
    size_t length;
    size_t capacity;
} CompletionList;

enum
{
    COMPLETION_CONFIRMATION_LIMIT = 20
};

static int reserve_line(char **line,
                        size_t *capacity,
                        size_t required);
static int print_completions(const CompletionList *matches);
static int request_completion_confirmation(size_t count);

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

static int completion_add(CompletionList *list,
                          const char *prefix,
                          const char *name,
                          char suffix)
{
    size_t prefix_length = strlen(prefix);
    size_t name_length = strlen(name);
    if (prefix_length > (size_t)-1 - name_length - 2)
    {
        errno = ENOMEM;
        return -1;
    }
    size_t candidate_length = prefix_length + name_length;
    char *candidate = malloc(candidate_length + 2);
    if (candidate == NULL)
    {
        return -1;
    }
    memcpy(candidate, prefix, prefix_length);
    memcpy(candidate + prefix_length, name, name_length);
    candidate[candidate_length] = suffix;
    candidate[candidate_length + 1] = '\0';

    for (size_t index = 0; index < list->length; index++)
    {
        if (strcmp(list->items[index], candidate) == 0)
        {
            free(candidate);
            return 0;
        }
    }
    if (list->length == list->capacity)
    {
        size_t next_capacity =
            list->capacity == 0 ? 8 : list->capacity * 2;
        if (next_capacity <= list->capacity
            || next_capacity
                   > (size_t)-1 / sizeof(*list->items))
        {
            free(candidate);
            errno = ENOMEM;
            return -1;
        }
        char **items =
            realloc(list->items,
                    next_capacity * sizeof(*items));
        if (items == NULL)
        {
            free(candidate);
            return -1;
        }
        list->items = items;
        list->capacity = next_capacity;
    }
    list->items[list->length++] = candidate;
    return 0;
}

static void completion_destroy(CompletionList *list)
{
    for (size_t index = 0; index < list->length; index++)
    {
        free(list->items[index]);
    }
    free(list->items);
}

static int has_prefix(const char *value, const char *prefix)
{
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

static int scan_directory(CompletionList *list,
                          const char *directory,
                          const char *candidate_prefix,
                          const char *name_prefix,
                          int commands)
{
    DIR *stream = opendir(directory);
    if (stream == NULL)
    {
        return 0;
    }

    struct dirent *entry;
    int result = 0;
    while ((entry = readdir(stream)) != NULL)
    {
        if ((entry->d_name[0] == '.'
             && name_prefix[0] != '.')
            || !has_prefix(entry->d_name, name_prefix))
        {
            continue;
        }

        size_t directory_length = strlen(directory);
        size_t name_length = strlen(entry->d_name);
        char *full_path =
            malloc(directory_length + name_length + 2);
        if (full_path == NULL)
        {
            result = -1;
            break;
        }
        snprintf(full_path,
                 directory_length + name_length + 2,
                 "%s/%s",
                 directory,
                 entry->d_name);

        struct stat info;
        int is_directory = stat(full_path, &info) == 0
                           && S_ISDIR(info.st_mode);
        if (commands)
        {
            if (!is_directory
                && access(full_path, X_OK) == 0
                && completion_add(list,
                                  candidate_prefix,
                                  entry->d_name,
                                  ' ')
                       != 0)
            {
                result = -1;
            }
        }
        else if (completion_add(list,
                                candidate_prefix,
                                entry->d_name,
                                is_directory ? '/' : ' ')
                 != 0)
        {
            result = -1;
        }
        free(full_path);
        if (result != 0)
        {
            break;
        }
    }
    closedir(stream);
    return result;
}

static int collect_command_completions(CompletionList *list,
                                       const char *prefix)
{
    static const char *builtins[] = {"cd", "exit", "pwd"};
    for (size_t index = 0;
         index < sizeof(builtins) / sizeof(builtins[0]);
         index++)
    {
        if (has_prefix(builtins[index], prefix)
            && completion_add(list,
                              "",
                              builtins[index],
                              ' ')
                   != 0)
        {
            return -1;
        }
    }

    const char *path = getenv("PATH");
    if (path == NULL)
    {
        return 0;
    }
    char *path_copy = strdup(path);
    if (path_copy == NULL)
    {
        return -1;
    }

    char *directory = path_copy;
    int result = 0;
    while (directory != NULL)
    {
        char *next = strchr(directory, ':');
        if (next != NULL)
        {
            *next = '\0';
        }
        result = scan_directory(
            list,
            directory[0] == '\0' ? "." : directory,
            "",
            prefix,
            1);
        if (result != 0 || next == NULL)
        {
            break;
        }
        directory = next + 1;
    }
    free(path_copy);
    return result;
}

static int collect_path_completions(CompletionList *list,
                                    const char *prefix)
{
    const char *slash = strrchr(prefix, '/');
    size_t directory_prefix_length =
        slash == NULL ? 0 : (size_t)(slash - prefix) + 1;
    const char *name_prefix =
        prefix + directory_prefix_length;
    char *directory = NULL;
    if (directory_prefix_length == 0)
    {
        directory = strdup(".");
    }
    else
    {
        size_t directory_length =
            directory_prefix_length - 1;
        if (directory_length == 0)
        {
            directory_length = 1;
        }
        directory = malloc(directory_length + 1);
        if (directory != NULL)
        {
            memcpy(directory, prefix, directory_length);
            directory[directory_length] = '\0';
        }
    }
    if (directory == NULL)
    {
        return -1;
    }
    char *candidate_prefix =
        malloc(directory_prefix_length + 1);
    if (candidate_prefix == NULL)
    {
        free(directory);
        return -1;
    }
    memcpy(candidate_prefix,
           prefix,
           directory_prefix_length);
    candidate_prefix[directory_prefix_length] = '\0';
    int result = scan_directory(list,
                                directory,
                                candidate_prefix,
                                name_prefix,
                                0);
    free(candidate_prefix);
    free(directory);
    return result;
}

static int compare_completions(const void *left,
                               const void *right)
{
    const char *const *left_item = left;
    const char *const *right_item = right;
    return strcmp(*left_item, *right_item);
}

static int command_position(const char *line,
                            size_t word_start)
{
    while (word_start > 0)
    {
        char character = line[--word_start];
        if (isspace((unsigned char)character))
        {
            continue;
        }
        return character == ';';
    }
    return 1;
}

static int complete_line(char **line,
                         size_t *capacity,
                         size_t *length,
                         size_t *cursor,
                         int confirm_large_list,
                         int *confirm_next_tab)
{
    *confirm_next_tab = 0;
    size_t word_start = *cursor;
    while (
        word_start > 0
        && !isspace((unsigned char)(*line)[word_start - 1])
        && (*line)[word_start - 1] != ';')
    {
        word_start--;
    }
    size_t prefix_length = *cursor - word_start;
    char *prefix = malloc(prefix_length + 1);
    if (prefix == NULL)
    {
        return -1;
    }
    memcpy(prefix, *line + word_start, prefix_length);
    prefix[prefix_length] = '\0';
    if (prefix_length == 0)
    {
        free(prefix);
        write_all("\a", 1);
        return 0;
    }

    CompletionList matches = {0};
    int result;
    if (command_position(*line, word_start)
        && strchr(prefix, '/') == NULL)
    {
        result =
            collect_command_completions(&matches, prefix);
    }
    else
    {
        result = collect_path_completions(&matches, prefix);
    }
    free(prefix);
    if (result != 0)
    {
        completion_destroy(&matches);
        return -1;
    }
    if (matches.length == 0)
    {
        write_all("\a", 1);
        completion_destroy(&matches);
        return 0;
    }

    qsort(matches.items,
          matches.length,
          sizeof(*matches.items),
          compare_completions);
    size_t common_length = strlen(matches.items[0]);
    for (size_t index = 1; index < matches.length; index++)
    {
        size_t character = 0;
        while (character < common_length
               && matches.items[0][character]
                      == matches.items[index][character])
        {
            character++;
        }
        common_length = character;
    }

    const char *replacement = matches.items[0];
    size_t replacement_length = common_length;
    if (matches.length > 1
        && common_length <= prefix_length)
    {
        if (matches.length > COMPLETION_CONFIRMATION_LIMIT
            && !confirm_large_list)
        {
            *confirm_next_tab = 1;
            int display_result =
                request_completion_confirmation(
                    matches.length);
            completion_destroy(&matches);
            return display_result;
        }
        int display_result = print_completions(&matches);
        completion_destroy(&matches);
        return display_result;
    }
    if (matches.length == 1)
    {
        replacement_length = strlen(replacement);
    }
    size_t suffix_length = *length - *cursor;
    size_t new_length =
        word_start + replacement_length + suffix_length;
    if (reserve_line(line, capacity, new_length + 1) != 0)
    {
        completion_destroy(&matches);
        return -1;
    }
    memmove(*line + word_start + replacement_length,
            *line + *cursor,
            suffix_length + 1);
    memcpy(*line + word_start,
           replacement,
           replacement_length);
    *length = new_length;
    *cursor = word_start + replacement_length;
    completion_destroy(&matches);
    return 0;
}

static int print_completions(const CompletionList *matches)
{
    if (write_all("\r\n", 2) != 0)
    {
        return -1;
    }
    for (size_t index = 0; index < matches->length; index++)
    {
        size_t length = strlen(matches->items[index]);
        if (length > 0
            && matches->items[index][length - 1] == ' ')
        {
            length--;
        }
        if (write_all(matches->items[index], length) != 0
            || write_all("  ", 2) != 0)
        {
            return -1;
        }
    }
    return write_all("\r\n", 2);
}

static int request_completion_confirmation(size_t count)
{
    char message[128];
    int length = snprintf(
        message,
        sizeof(message),
        "\r\n%zu matches. Press Tab again to show all; "
        "any other key cancels.\r\n",
        count);
    if (length < 0 || (size_t)length >= sizeof(message))
    {
        errno = EOVERFLOW;
        return -1;
    }
    return write_all(message, (size_t)length);
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

    size_t entry_count = history_get_count(history);
    for (size_t index = entry_count; index > 0; index--)
    {
        fprintf(file,
                "%s\n",
                history_get_entry(history, index - 1));
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
    int confirm_completions = 0;
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

        if (character == '\t')
        {
            int confirm_next_tab = 0;
            if (complete_line(line,
                              capacity,
                              &length,
                              &cursor,
                              confirm_completions,
                              &confirm_next_tab)
                    != 0
                || redraw_line(prompt_buffer,
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
            confirm_completions = confirm_next_tab;
            continue;
        }
        confirm_completions = 0;

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