#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "shit/environment/environment.h"
#include "shit/expansion/expansion.h"

static int is_name_start(char character)
{
    return (character >= 'A' && character <= 'Z')
           || (character >= 'a' && character <= 'z')
           || character == '_';
}

static int is_name_character(char character)
{
    return is_name_start(character)
           || (character >= '0' && character <= '9');
}

static int append_text(char **buffer,
                       size_t *length,
                       size_t *capacity,
                       const char *text,
                       size_t text_length)
{
    if (text_length > (size_t)-1 - *length - 1)
    {
        errno = ENOMEM;
        return -1;
    }
    size_t required = *length + text_length + 1;
    if (required > *capacity)
    {
        size_t next_capacity = *capacity;
        while (next_capacity < required)
        {
            if (next_capacity > (size_t)-1 / 2)
            {
                next_capacity = required;
                break;
            }
            next_capacity *= 2;
        }
        char *next_buffer = realloc(*buffer, next_capacity);
        if (next_buffer == NULL)
        {
            return -1;
        }
        *buffer = next_buffer;
        *capacity = next_capacity;
    }
    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';
    return 0;
}

static int append_variable(char **buffer,
                           size_t *length,
                           size_t *capacity,
                           const char *name,
                           size_t name_length)
{
    char *variable_name = malloc(name_length + 1);
    if (variable_name == NULL)
    {
        return -1;
    }
    memcpy(variable_name, name, name_length);
    variable_name[name_length] = '\0';
    const char *value = environment_get(variable_name);
    free(variable_name);
    if (value == NULL)
    {
        value = "";
    }
    return append_text(buffer,
                       length,
                       capacity,
                       value,
                       strlen(value));
}

static int expand_word(const char *word, char **result)
{
    size_t capacity = strlen(word) + 1;
    char *expanded = malloc(capacity);
    if (expanded == NULL)
    {
        return -1;
    }
    size_t length = 0;
    expanded[0] = '\0';

    for (size_t position = 0; word[position] != '\0';)
    {
        if (word[position] != '$')
        {
            if (append_text(&expanded,
                            &length,
                            &capacity,
                            word + position,
                            1)
                != 0)
            {
                free(expanded);
                return -1;
            }
            position++;
            continue;
        }

        size_t name_start = position + 1;
        size_t name_end = name_start;
        size_t next_position = name_start;
        if (word[name_start] == '{')
        {
            name_start++;
            name_end = name_start;
            while (word[name_end] != '\0'
                   && word[name_end] != '}')
            {
                name_end++;
            }
            if (word[name_end] != '}'
                || !is_name_start(word[name_start]))
            {
                free(expanded);
                errno = EINVAL;
                return -1;
            }
            for (size_t index = name_start + 1;
                 index < name_end;
                 index++)
            {
                if (!is_name_character(word[index]))
                {
                    free(expanded);
                    errno = EINVAL;
                    return -1;
                }
            }
            next_position = name_end + 1;
        }
        else if (is_name_start(word[name_start]))
        {
            name_end = name_start + 1;
            while (is_name_character(word[name_end]))
            {
                name_end++;
            }
            next_position = name_end;
        }
        else
        {
            if (append_text(&expanded,
                            &length,
                            &capacity,
                            "$",
                            1)
                != 0)
            {
                free(expanded);
                return -1;
            }
            position++;
            continue;
        }

        if (append_variable(&expanded,
                            &length,
                            &capacity,
                            word + name_start,
                            name_end - name_start)
            != 0)
        {
            free(expanded);
            return -1;
        }
        position = next_position;
    }

    *result = expanded;
    return 0;
}

int expansion_expand_command(Command *command)
{
    for (size_t index = 0; index < command->argc; index++)
    {
        char *expanded = NULL;
        if (expand_word(command->argv[index], &expanded)
            != 0)
        {
            return -1;
        }
        free(command->argv[index]);
        command->argv[index] = expanded;
    }
    return 0;
}