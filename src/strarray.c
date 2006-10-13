/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>

#include "compatibility.h"
#include "strarray.h"

void strarray_init (strarray_t *sa) {
	sa->strings = NULL;
	sa->count = 0;
	sa->max = 0;
}

void strarray_cleanup (strarray_t *sa) {
	int i;
	for (i=0; i<sa->count; i++)
		free (sa->strings[i]);
	free (sa->strings);
}

int strarray_append (strarray_t *sa, char *str) {
	// grow the array if needed
	if (sa->count == sa->max) {
		if (sa->max < 10)
			sa->max = 10;
		else
			sa->max += sa->max/2;
		sa->strings = realloc (sa->strings, sa->max * sizeof (char*));
	}
	// append the string
	sa->strings[sa->count++] = str;
	sa->strings[sa->count] = NULL;
	return sa->count;
}

int strarray_append_dup (strarray_t *sa, const char *str) {
	return strarray_append (sa, strdup (str));
}

int strarray_append_ndup (strarray_t *sa, const char *str, unsigned len) {
	return strarray_append (sa, strndup (str, len));
}

int strarray_append_array_dup (strarray_t *sa, const strarray_t *sa2) {
	int i, ret = sa->count;
	for (i=0; i < sa2->count; i++) {
		ret = strarray_append_dup (sa, sa2->strings[i]);
		if (ret < 0)
			break;
	}
	return ret;
}

int strarray_append_split (strarray_t *sa, const char *str, char split_on) {
	const char *p, *c;

	if (!str)
		return -1;

	p = str;
	while ( (c = strchr (p, split_on)) ) {
		strarray_append_ndup (sa, p, c-p);
		p = c + 1;
	}

	return strarray_append_dup (sa, p);
}

