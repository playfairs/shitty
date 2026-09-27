#ifndef SHIT_HISTORY_H
#define SHIT_HISTORY_H

#include <stddef.h>

typedef struct
{
    char **entries;
    size_t length;
    size_t capacity;
    size_t position;
    char *saved_line;
    char *file_path;
    size_t max_entries;
} History;

void history_initialize(History *history);
void history_destroy(History *history);
int history_add(History *history, const char *line);
const char *history_previous(History *history,
                             const char *current_line);
const char *history_next(History *history);
void history_reset_navigation(History *history);
int history_load(History *history);
int history_save(History *history);
const char *history_get_entry(const History *history,
                              size_t index);
size_t history_get_count(const History *history);

#endif