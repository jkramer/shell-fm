/*
	vim:syntax=c tabstop=2 shiftwidth=2
	
	Shell.FM - getline.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>

unsigned getln(char ** ptr, unsigned * size, FILE * fd) {
	unsigned length = 0;
	char ch = 0;

	if(!(* ptr))
		* size = 0;
	
	while(!feof(fd) && ch != (char) 10) {
		ch = fgetc(fd);
		
		if(length + 2 > * size)
			assert((* ptr = realloc(* ptr, (* size += 1024))) != NULL);

		(* ptr)[length++] = ch;
		(* ptr)[length] = 0;
	}

	* size = length + 1;
	* ptr = realloc(* ptr, * size);

	return length;
}
