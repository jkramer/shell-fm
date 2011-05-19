/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_SETTINGS
#define SHELLFM_SETTINGS

#define PACKAGE_VERSION "0.8"

#include "hash.h"

extern int settings(const char *, int);
extern const char * rcpath(const char *);
extern void makercd(void);

extern struct hash rc;

#endif
