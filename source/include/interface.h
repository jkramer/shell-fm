/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_INTERFACE
#define SHELLFM_INTERFACE

#include "hash.h"

#define M_COLORED   0x1
#define M_RELAXPATH 0x2
#define M_SHELLESC	0x4

extern const char * meta(const char *, int, struct hash *);
extern void interface(int);
extern void run(const char *);
extern void canon(int);
extern int fetchkey(unsigned);
extern void shownp(void);
extern void tag(struct hash);
extern int rate(const char *);

#endif
