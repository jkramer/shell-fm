/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_ROPEN
#define SHELLFM_ROPEN

#include <stdio.h>

extern FILE * ropen(const char *, unsigned short);
extern void fshutdown(FILE **);

#endif
