
#ifndef SHELLFM_UTIL
#define SHELLFM_UTIL

extern char ** slurp(const char *);
extern char ** uniq(char **);
extern int grep(char **, char *);
extern char * strcasestr(const char *, const char *);
extern char * strjoin(const char *, ...);
extern void spit(const char *, const char *);
extern const char const * plain_md5(const char *);

#if ((defined(__FreeBSD__) || defined(__OpenBSD__)) || defined(__darwin__) || (defined (__SVR4) && defined (__sun)) && !defined(__STRNDUP__))
#define __STRNDUP__
extern char * strndup(const char *, size_t);
#endif

void debug(char *, ...);

#endif

