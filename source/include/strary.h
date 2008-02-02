/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_STRARRAY
#define SHELLFM_STRARRAY

#include <string.h>
#include <stdlib.h>

extern unsigned count(char **);
extern char ** append(char **, const char *);
extern char ** merge(char **, char **, int);
extern void purge(char **);
extern char * join(char **, int);

#endif

