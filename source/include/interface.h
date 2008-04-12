/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_INTERFACE
#define SHELLFM_INTERFACE

#include "hash.h"

extern const char * meta(const char *, int, struct hash *);
extern void interface(int);
extern void run(const char *);
extern void canon(int);
extern int fetchkey(unsigned);
extern void shownp(void);
extern void tag(struct hash);
extern int rate(const char *);

#endif
