#ifndef SHIT_INPUT_H
#define SHIT_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "shit/core/shell.h"

typedef enum
{
    INPUT_ERROR = -1,
    INPUT_EOF = 0,
    INPUT_LINE = 1,
    INPUT_INTERRUPTED = 2
} InputResult;

InputResult input_read_line(bool interactive,
                            Shell *shell,
                            char **line,
                            size_t *capacity);

#endif