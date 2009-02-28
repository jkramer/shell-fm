
#ifndef SHELLFM_UTIL
#define SHELLFM_UTIL

extern char ** slurp(const char *);
extern char ** uniq(char **);
extern int grep(char **, char *);
extern char * strcasestr(const char *, const char *);

#if ((defined(__FreeBSD__) || defined(__OpenBSD__)) && !defined(__STRNDUP__))
#define __STRNDUP__
extern char * strndup(const char *, size_t);
#endif

#endif

