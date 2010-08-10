/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_SCKIF
#define SHELLFM_SCKIF

extern int tcpsock(const char *, unsigned short);
extern int unixsock(const char *);

extern void rmsckif(void);

extern void sckif(int, int);

extern void execcmd(const char *, char *);

extern void accept_client(int);
extern void handle_client(int);

#endif
