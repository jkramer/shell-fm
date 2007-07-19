/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include "compatibility.h"
#include "strary.h"

/* Initialize a string array structure. */
void mkstrary(struct strary * sa) {
	sa->strings = NULL;
	sa->count = 0;
	sa->max = 0;
}

/* Free and clear a string array structure. */
void rmstrary(struct strary * sa) {
	while(sa->count--)
		free(sa->strings[sa->count]);

	free(sa->strings);
	sa->strings = NULL;
}

/* Append a string to a string array. */
int strapp(struct strary * sa, char * str) {
	/* Grow the array if needed. */
	if(sa->count == sa->max) {
		if(sa->max < 10)
			sa->max = 10;
		else
			sa->max += (sa->max / 2);

		sa->strings = realloc(sa->strings, sa->max * sizeof (char *));
		assert(sa->strings != NULL);
	}

	/* Append the string. */
	sa->strings[sa->count++] = str;
	sa->strings[sa->count] = NULL;

	return sa->count;
}

/* strdup a string a append the copy to a string array. */
int strappdup(struct strary * sa, const char * str) {
	return strapp(sa, strdup(str));
}

/* strndup a string a append the copy to a string array. */
int strappndup(struct strary * sa, const char * str, unsigned len) {
	return strapp(sa, strndup(str, len));
}

/* strdup the strings of a string array and append them to another one. */
int straryapp(struct strary * dst, const struct strary * src) {
	unsigned i;
	int ret = dst->count;
	for(i = 0; i < src->count; ++i) {
		ret = strappdup(dst, src->strings[i]);
		if(ret < 0)
			break;
	}

	return ret;
}

/* Split a string at a given character and put the strings into an array. */
int strappsplt(struct strary * sa, const char * str, char delim) {
	const char * p, * c;

	if(str == NULL)
		return -1;

	p = str;
	while((c = strchr(p, delim))) {
		strappndup(sa, p, c - p);
		p = c + 1;
	}

	return strappdup(sa, p);
}
