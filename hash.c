/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - hash.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "include/hash.h"


void set(struct hash * hash, const char * key, const char * value) {
	unsigned index;
	
	index = haskey(hash, key);
	if(index--) {
		hash->content[index].value =
			realloc(hash->content[index].value, strlen(value) + 1);
		
		assert(hash->content[index].value != NULL);
		
		memset(hash->content[index].value, 0, strlen(value + 1));
		strcpy(hash->content[index].value, value);
	} else {
		hash->content =
			realloc(hash->content, sizeof(struct pair) * (hash->size + 1));
		
		assert(hash->content != NULL);
		
		hash->content[hash->size].key = strdup(key);
		hash->content[hash->size].value = strdup(value);

		++hash->size;
	}
}

const char * value(struct hash * hash, const char * key) {
	unsigned index = haskey(hash, key);

	if(index)
		return hash->content[index - 1].value;
	
	return NULL;
}

void unset(struct hash * hash, const char * key) {
	unsigned index = haskey(hash, key);
	if(index--) {
		free(hash->content[index].key);
		free(hash->content[index].value);
		memcpy(
				& hash->content[index],
				& hash->content[hash->size - 1],
				sizeof(struct pair));
		hash->content = realloc(hash->content, sizeof(struct pair) * --hash->size);
		assert(hash->content);
	}
}

void empty(struct hash * hash) {
	if(hash->content) {
		while(hash->size--) {
			free(hash->content[hash->size].key);
			free(hash->content[hash->size].value);
		}
		free(hash->content);
		hash->content = NULL;
	}
}

int haskey(struct hash * hash, const char * key) {
	register unsigned x;
	
	if(!hash->size || !hash->content)
		return 0;
	
	for(x = 0; x < hash->size; ++x)
		if(!strcmp(hash->content[x].key, key))
			return x + 1;

	return 0;
}
