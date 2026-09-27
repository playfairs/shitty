#ifndef SHIT_SIGNALS_H
#define SHIT_SIGNALS_H

#include <stdbool.h>

int signals_initialize_shell(void);
void signals_prepare_child(void);
bool signals_was_interrupted(void);
void signals_clear_interrupt(void);

#endif