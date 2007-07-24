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


/* Counts the elements of a NULL-terminated array of strings. */
unsigned count(char ** list) {
	unsigned n = 0;

	if(list)
		while(list[n] != NULL)
			++n;

	return n;
}

/* Appends a string to a NULL-terminated array of strings. */
char ** append(char ** list, const char * string) {
	unsigned size = count(list);

	list = realloc(list, sizeof(char *) * (size + 2));

	list[size++] = strdup(string);
	list[size] = NULL;

	return list;
}

/*
	Merge two arrays of strings. If the third parameter is zero,
	the elements of the second array and the array itself are freed.
*/
char ** merge(char ** list, char ** appendix, int keep) {
	unsigned size = count(list), i;

	for(i = 0; appendix && appendix[i] != NULL; ++i) {
		list = realloc(list, sizeof(char *) * (size + 2));
		list[size++] = strdup(appendix[i]);
		list[size] = NULL;

		if(!keep)
			free(appendix[i]);
	}

	if(appendix != NULL && !keep)
		free(appendix);

	return list;
}


/* Free a NULL-terminated array of strings. */
void purge(char ** list) {
	unsigned i = 0;

	if(list != NULL) {
		while(list[i] != NULL)
			free(list[i++]);

		free(list);
	}
}
