#include <stdio.h>
#include <unistd.h>

#include "shit/prompt/prompt.h"

static const char *color_reset = "\033[0m";
static const char *color_lambda = "\033[1;36m";
static const char *color_separator = "\033[0;37m";

void prompt_initialize(PromptConfig *config)
{
    config->use_color = isatty(STDOUT_FILENO) != 0;
}

void prompt_render(const PromptConfig *config,
                   char *buffer,
                   size_t capacity)
{
    if (config->use_color)
    {
        snprintf(buffer,
                 capacity,
                 "%sλ%s : %s",
                 color_lambda,
                 color_separator,
                 color_reset);
    }
    else
    {
        snprintf(buffer, capacity, "λ : ");
    }
}

const char *prompt_get_color_reset(void)
{
    return color_reset;
}

const char *prompt_get_lambda_color(void)
{
    return color_lambda;
}

const char *prompt_get_separator_color(void)
{
    return color_separator;
}
