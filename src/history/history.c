#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shit/history/history.h"

static const char *get_history_path(void)
{
    static char path[512];
    const char *home = getenv("HOME");
    if (home == NULL)
    {
        return NULL;
    }
    snprintf(path, sizeof(path), "%s/.shit_history", home);
    return path;
}

void history_initialize(History *history)
{
    *history = (History){.max_entries = 1000};
}

void history_destroy(History *history)
{
    for (size_t index = 0; index < history->length; index++)
    {
        free(history->entries[index]);
    }
    free(history->entries);
    free(history->saved_line);
    free(history->file_path);
    *history = (History){.max_entries = 1000};
}

void history_reset_navigation(History *history)
{
    free(history->saved_line);
    history->saved_line = NULL;
    history->position = history->length;
}

int history_add(History *history, const char *line)
{
    if (line[0] == '\0')
    {
        history_reset_navigation(history);
        return 0;
    }
    if (history->length > 0
        && strcmp(history->entries[history->length - 1],
                  line)
               == 0)
    {
        history_reset_navigation(history);
        return 0;
    }
    if (history->length == history->capacity)
    {
        size_t next_capacity = history->capacity == 0
                                   ? 16
                                   : history->capacity * 2;
        if (next_capacity <= history->capacity
            || next_capacity
                   > (size_t)-1 / sizeof(*history->entries))
        {
            return -1;
        }
        char **entries =
            realloc(history->entries,
                    next_capacity * sizeof(*entries));
        if (entries == NULL)
        {
            return -1;
        }
        history->entries = entries;
        history->capacity = next_capacity;
    }
    char *entry = strdup(line);
    if (entry == NULL)
    {
        return -1;
    }
    history->entries[history->length++] = entry;
    history_reset_navigation(history);
    return 0;
}

const char *history_previous(History *history,
                             const char *current_line)
{
    if (history->length == 0 || history->position == 0)
    {
        return NULL;
    }
    if (history->position == history->length)
    {
        char *saved_line = strdup(current_line);
        if (saved_line == NULL)
        {
            return NULL;
        }
        free(history->saved_line);
        history->saved_line = saved_line;
    }
    history->position--;
    return history->entries[history->position];
}

const char *history_next(History *history)
{
    if (history->position >= history->length)
    {
        return NULL;
    }
    history->position++;
    if (history->position == history->length)
    {
        return history->saved_line == NULL
                   ? ""
                   : history->saved_line;
    }
    return history->entries[history->position];
}

int history_load(History *history)
{
    const char *path = get_history_path();
    if (path == NULL)
    {
        return -1;
    }
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        return 0;
    }
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;
    while ((length = getline(&line, &capacity, file)) != -1)
    {
        if (length > 0 && line[length - 1] == '\n')
        {
            line[length - 1] = '\0';
        }
        if (line[0] != '\0')
        {
            history_add(history, line);
        }
    }
    free(line);
    fclose(file);
    return 0;
}

int history_save(History *history)
{
    const char *path = get_history_path();
    if (path == NULL)
    {
        return -1;
    }
    FILE *file = fopen(path, "w");
    if (file == NULL)
    {
        return -1;
    }
    for (size_t index = 0; index < history->length; index++)
    {
        fprintf(file, "%s\n", history->entries[index]);
    }
    fclose(file);
    return 0;
}

const char *history_get_entry(const History *history,
                              size_t index)
{
    if (index >= history->length)
    {
        return NULL;
    }
    return history->entries[index];
}

size_t history_get_count(const History *history)
{
    return history->length;
}
