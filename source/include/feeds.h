
#ifndef SHELLFM_FEEDS
#define SHELLFM_FEEDS

#include "hash.h"

extern char ** neighbors(const char *);
extern char ** topartists(const char *);
extern char ** friends(const char *);
extern char ** toptags(char, struct hash);
extern char ** overalltags(void);
extern char ** usertags(const char *);

#endif
