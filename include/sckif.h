
#ifndef SHELLFM_SCKIF
#define SHELLFM_SCKIF

extern int mksckif(const char *, unsigned short);
extern void rmsckif(void);
extern void sckif(void);

extern void execcmd(const char *, FILE *);

#endif
