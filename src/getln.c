/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
	
	Shell.FM - getline.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>

#include "getln.h"

unsigned getln(char ** ptr, unsigned * size, FILE * fd) {
	unsigned length = 0;
	int ch = 0;

	if(!(* ptr))
		* size = 0;
	
	while(!feof(fd) && ch != (char) 10) {
		ch = fgetc(fd);

		if(ch == -1)
			ch = 0;
		
		if(length + 2 > * size) {
			* ptr = realloc(* ptr, (* size += 1024));
			assert(* ptr);
		}

		(* ptr)[length++] = (char) ch;
		(* ptr)[length] = 0;
	}

	* size = length + 1;
	* ptr = realloc(* ptr, * size);

	return length;
}
