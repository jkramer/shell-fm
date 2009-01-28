/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "getln.h"

/*
	Reads a line until EOF or a line break occurs, whatever comes first.
	The first parameter must be a pointer to another pointer which should either
	be NULL or point to memory area reserved with *alloc. If it's a pointer to a
	memory area, that memory will be used and resized as needed. In this case,
	the second argument must point to an unsigned holding the size of the
	allocated memory area. Otherwise, the memory will be freshly allocated and
	it's size will be stored in the unsigned pointed to by the second argument.
	The third argument is a FILE pointer which the line will be read from.
*/
unsigned getln(char ** ptr, unsigned * size, FILE * fd) {
	unsigned length = 0;
	int ch = 0;

	assert(size != NULL);

	if(!(* ptr))
		* size = 0;
	
	while(!feof(fd) && ch != (char) 10) {
		ch = fgetc(fd);

		if(ch == -1)
			ch = 0;
		
		if(length + 2 > * size) {
			* ptr = realloc(* ptr, (* size += 1024));
			assert(* ptr != NULL);
		}

		(* ptr)[length++] = (char) ch;
		(* ptr)[length] = 0;
	}

	* size = length + 1;
	* ptr = realloc(* ptr, * size);

	assert(* ptr != NULL);

	return length;
}
