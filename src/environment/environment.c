#include <stdlib.h>

#include "shit/environment/environment.h"

const char *environment_get(const char *name)
{
    return getenv(name);
}

int environment_set(const char *name, const char *value)
{
    return setenv(name, value, 1);
}