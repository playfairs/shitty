#ifndef SHIT_PROMPT_H
#define SHIT_PROMPT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    bool use_color;
} PromptConfig;

void prompt_initialize(PromptConfig *config);
void prompt_render(const PromptConfig *config,
                   char *buffer,
                   size_t capacity);
const char *prompt_get_color_reset(void);
const char *prompt_get_lambda_color(void);
const char *prompt_get_separator_color(void);

#endif
