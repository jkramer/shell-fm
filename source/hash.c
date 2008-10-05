/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "hash.h"


void set(struct hash * hash, const char * key, const char * value) {
	unset(hash, key);

	pthread_mutex_lock(& hash->mutex);

	hash->content =
		realloc(hash->content, sizeof(struct pair) * (hash->size + 1));
	
	assert(hash->content != NULL);
	
	assert((hash->content[hash->size].key = strdup(key)) != NULL);
	assert((hash->content[hash->size].value = strdup(value)) != NULL);

	++hash->size;

	pthread_mutex_unlock(& hash->mutex);
}

const char * value(struct hash * hash, const char * key) {
	unsigned index;

	index = haskey(hash, key);

	return index > 0 ? hash->content[index - 1].value : "";
}

void unset(struct hash * hash, const char * key) {
	unsigned index;

	pthread_mutex_lock(& hash->mutex);
	
	index = haskey(hash, key);
	if(index > 0) {
		--index;

		if(hash->content[index].key != NULL)
			free(hash->content[index].key);

		if(hash->content[index].value != NULL)
			free(hash->content[index].value);

		memcpy(
				& hash->content[index],
				& hash->content[--hash->size],
				sizeof(struct pair));

		hash->content = realloc(hash->content, sizeof(struct pair) * hash->size);
		assert(hash->content != NULL);
	}

	pthread_mutex_unlock(& hash->mutex);
}

void empty(struct hash * hash) {
	pthread_mutex_lock(& hash->mutex);

	if(hash != NULL) {
		if(hash->content != NULL) {
			while(hash->size > 0) {
				--hash->size;
				if(hash->content[hash->size].key != NULL)
					free(hash->content[hash->size].key);

				if(hash->content[hash->size].value != NULL)
					free(hash->content[hash->size].value);
			}
			free(hash->content);
		}
		memset(hash, 0, sizeof(struct hash));
	}

	pthread_mutex_unlock(& hash->mutex);
}

int haskey(struct hash * hash, const char * key) {
	register unsigned x;

	assert(hash != NULL);
	assert(key != NULL);
	
	if(!hash->size || !hash->content) {
		pthread_mutex_unlock(& hash->mutex);
		return 0;
	}
	
	for(x = 0; x < hash->size; ++x)
		if(hash->content[x].key != NULL && !strcmp(hash->content[x].key, key)) {
			pthread_mutex_unlock(& hash->mutex);
			return x + 1;
		}

	return 0;
}
