
#ifndef SHELLFM_ROPEN
#define SHELLFM_ROPEN

#include <stdio.h>

extern FILE * ropen(const char *, unsigned short);
extern void fshutdown(FILE *);

#endif
