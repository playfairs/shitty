#ifndef SHIT_TERMINAL_H
#define SHIT_TERMINAL_H

#include <termios.h>

int terminal_begin_input(struct termios *original);
int terminal_end_input(const struct termios *original);

#endif