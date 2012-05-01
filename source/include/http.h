/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_HTTP
#define SHELLFM_HTTP

#include <stdio.h>
#include <sys/time.h>

#include "hash.h"

struct content_handle {
	FILE * fd;

	int chunked, left;
};

extern char ** fetch(const char *, struct content_handle *, const char *, const char *);
extern char ** cache(const char *, const char *, int);
extern unsigned encode(const char *, char **);
extern unsigned decode(const char *, char **);
extern const char * makeurl(const char *, ...);
extern void unhtml(char *);
extern void lag(time_t);
extern void freeln(char **, unsigned *);
extern int receive(struct content_handle *, char *, int);

#endif
