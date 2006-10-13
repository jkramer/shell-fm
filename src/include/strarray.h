/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
*/

#ifndef SHELLFM_STRARRAY
#define SHELLFM_STRARRAY

#include <string.h>
#include <stdlib.h>

typedef struct strarray_s {
	unsigned count;	// number of strings in array
	unsigned max;		// number of strings we have room for
	char **strings;	// array large enough for max+1 char* pointers (last is NULL)
} strarray_t;

extern void strarray_init (strarray_t *sa);
extern void strarray_cleanup (strarray_t *sa);

extern int strarray_append (strarray_t *sa, char *str);
extern int strarray_append_dup (strarray_t *sa, const char *str);
extern int strarray_append_ndup (strarray_t *sa, const char *str, unsigned len);
extern int strarray_append_array_dup (strarray_t *sa, const strarray_t *sa2);
extern int strarray_append_split (strarray_t *sa, const char *str, char split_on);

#define strarray_for(sa,n,str) \
	for((n)=0; (n)<(sa)->count; (n)++)

#endif

