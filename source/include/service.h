/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_SERVICE
#define SHELLFM_SERVICE

#include "hash.h"
#include "playlist.h"

extern int authenticate(const char *, const char *);
extern int station(const char *);
extern int update(struct hash *);

extern int play(struct playlist *);

extern int delayquit;

#endif
