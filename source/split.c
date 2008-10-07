/*
	Copyright (C) 2006-2008 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/


#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "split.h"

/*
   Split a string wherever the given delimiter occurs, return a
   NULL-terminated array of strings with the chunks.
*/
char ** split(char * string, const char * del, unsigned * pnsplt) {
	char ** splt = NULL;
	unsigned nsplt = 0;
	register char * ptr = string;

	if(!string)
		return NULL;
	
	while(* ptr) {
		while(* ptr && strchr(del, * ptr))
			++ptr;
		if(* ptr) {
			register unsigned length = 0;
			splt = realloc(splt, sizeof(char *) * (nsplt + 2));
			assert(splt != NULL);
			splt[nsplt] = calloc(strlen(ptr) + 1, sizeof(char));
			assert(splt[nsplt] != NULL);
			while(* ptr && !strchr(del, * ptr))
				splt[nsplt][length++] = * (ptr++);
			splt[nsplt] = realloc(splt[nsplt], length + 1);
			splt[++nsplt] = NULL;
		}
	}

	if(pnsplt)
		* pnsplt = nsplt;

	return splt;
}
