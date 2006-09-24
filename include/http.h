
#ifndef SHELLFM_HTTP
#define SHELLFM_HTTP

extern char ** fetch(const char *, FILE **);
extern unsigned encode(const char *, char **);
extern unsigned decode(const char *, char **);

#endif
