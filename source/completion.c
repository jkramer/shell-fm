/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <stdarg.h>

#include <assert.h>

#include "compatibility.h"
#include "completion.h"


const char * nextmatch(char ** list, char * needle, unsigned * nres) {
	static char lastneedle[64] = { 0 };
	static int lastmatch = 0, matches = 0, nlen = 0;
	register unsigned i;

	if(!list)
		return NULL;

	/* Check if a new search is needed or wanted. */
	if(needle != NULL) {
		/* Remember needle for repeated calls. */
		memset(lastneedle, 0, sizeof(lastneedle));
		strncpy(lastneedle, needle, sizeof(lastneedle) - 1);

		/* Count number of matches of needle in list. */
		nlen = strlen(needle);
		for(matches = i = 0; list[i] != NULL; ++i)
			if(!strncasecmp(list[i], needle, nlen))
				++matches;

		/* Start search at first item. */
		i = 0;
	} else {
		/* Search for the last given needle. */
		needle = lastneedle;
		nlen = strlen(needle);

		/* Start search at first item after the last matched one. */
		i = lastmatch + 1;
	}

	if(nres != NULL)
		* nres = matches;

	if(matches) {
		while(matches > 0) {
			if(!list[i])
				i = 0;

			if(!strncasecmp(list[i], needle, nlen)) {
				lastmatch = i;
				return list[i];
			}

			++i;
		}
	}

	return NULL;
}
