#include <signal.h>
#include <stddef.h>

#include "shit/signals/signals.h"

static volatile sig_atomic_t interrupted;

static void handle_interrupt(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

int signals_initialize_shell(void)
{
    struct sigaction interrupt_action = {0};
    interrupt_action.sa_handler = handle_interrupt;
    if (sigemptyset(&interrupt_action.sa_mask) != 0
        || sigaction(SIGINT, &interrupt_action, NULL) != 0)
    {
        return -1;
    }

    struct sigaction quit_action = {0};
    quit_action.sa_handler = SIG_IGN;
    if (sigemptyset(&quit_action.sa_mask) != 0
        || sigaction(SIGQUIT, &quit_action, NULL) != 0)
    {
        return -1;
    }
    return 0;
}

void signals_prepare_child(void)
{
    struct sigaction default_action = {0};
    default_action.sa_handler = SIG_DFL;
    sigemptyset(&default_action.sa_mask);
    sigaction(SIGINT, &default_action, NULL);
    sigaction(SIGQUIT, &default_action, NULL);
}

bool signals_was_interrupted(void)
{
    return interrupted != 0;
}

void signals_clear_interrupt(void)
{
    interrupted = 0;
}