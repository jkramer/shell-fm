
#ifndef SHELLFM_SERVICE
#define SHELLFM_SERVICE

#include "hash.h"

extern int handshake(const char *, const char *);
extern int station(const char *);
extern int update(struct hash *);
extern int control(const char *);
extern int setdiscover(int);

#endif
