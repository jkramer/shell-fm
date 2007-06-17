/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
	
	Shell.FM - sckif.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "utility.h"


/* ------------------------------------------------------------------------ */

int matches_one_of (const char *word, ...) {
	va_list ap;
	const char *s;
	int matched = 0;

	va_start(ap, word);

	while ((s = va_arg(ap, char *))) {
		if (!strcmp(word, s)) {
			matched = 1;
			break;
		}
	}

	va_end(ap);

	return matched;
}

