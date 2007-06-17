/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_SCKIF
#define SHELLFM_SCKIF

extern int mksckif(const char *, unsigned short);
extern void rmsckif(void);
extern void sckif(int);

extern void execcmd(const char *, FILE *);

#endif
