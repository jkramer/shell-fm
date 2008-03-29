/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_HISTORY
#define SHELLFM_HISTORY

extern void histapp(const char *);
extern char ** slurp(const char *);
extern char ** uniq(char **);

#endif
