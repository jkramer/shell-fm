
#ifndef SHELLFM_GETLN
#define SHELLFM_GETLN

#include "http.h"

extern unsigned getln(char **, unsigned *, FILE *);
extern unsigned receive_line(char **, unsigned *, struct content_handle *);

#endif
