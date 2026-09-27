#ifndef SHIT_ENVIRONMENT_H
#define SHIT_ENVIRONMENT_H

const char *environment_get(const char *name);
int environment_set(const char *name, const char *value);

#endif