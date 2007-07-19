/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_STRARRAY
#define SHELLFM_STRARRAY

#include <string.h>
#include <stdlib.h>

struct strary {
	unsigned count; /* Number of strings in array. */
	unsigned max; /* Number of strings we have room for. */
	char ** strings; /* Array of strings (NULL-terminated). */
};

extern void mkstrary(struct strary *);
extern void rmstrary(struct strary *);

extern int strapp(struct strary *, char *);
extern int strappdup(struct strary *, const char *);
extern int strappndup(struct strary *, const char *, unsigned);
extern int straryapp(struct strary *, const struct strary *);
extern int strappsplt(struct strary *, const char *, char split_on);

#endif

