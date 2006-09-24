
#ifndef SHELLFM_HTTP
#define SHELLFM_HTTP

#include "hash.h"

extern char ** fetch(const char *, FILE **, char *);
extern unsigned encode(const char *, char **);
extern unsigned decode(const char *, char **);

#endif
