#include <unistd.h>

#include "shit/terminal/terminal.h"

int terminal_begin_input(struct termios *original)
{
    if (tcgetattr(STDIN_FILENO, original) != 0)
    {
        return -1;
    }
    struct termios input_mode = *original;
    input_mode.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    input_mode.c_cc[VMIN] = 1;
    input_mode.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &input_mode);
}

int terminal_end_input(const struct termios *original)
{
    return tcsetattr(STDIN_FILENO, TCSANOW, original);
}