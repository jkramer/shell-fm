
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "feeds.h"

char ** neighbors(const char * user) {
	char * encoded = NULL, feed[512], ** names = NULL;
	unsigned i;

	assert(user != NULL);

	encode(user, & encoded);

	memset(feed, (char) 0, sizeof(feed));
	snprintf(
		feed, sizeof(feed),
		"http://ws.audioscrobbler.com/1.0/user/%s/neighbours.txt",
		encoded
	);

	free(encoded);

	names = fetch(feed, NULL, NULL);

	if(names != NULL)
		for(i = 0; names[i] != NULL; ++i) {
			char * ptr = strchr(names[i], ',');
			if(ptr != NULL) {
				unsigned length = strlen(ptr + 1);
				memmove(names[i], ptr + 1, length);
				names[i][length] = 0;
				names[i] = realloc(names[i], sizeof(char) * (length + 1));
			}
		}

	return names;
}


char ** topartists(const char * user) {
	char * encoded = NULL, feed[512], ** names = NULL;
	unsigned i;

	assert(user != NULL);

	encode(user, & encoded);

	memset(feed, (char) 0, sizeof(feed));
	snprintf(
		feed, sizeof(feed),
		"http://ws.audioscrobbler.com/1.0/user/%s/topartists.txt",
		encoded
	);

	free(encoded);

	names = fetch(feed, NULL, NULL);

	if(names != NULL)
		for(i = 0; names[i] != NULL; ++i) {
			char * ptr = strchr(names[i], ',');
			if(ptr != NULL) {
				ptr = strchr(ptr + 1, ',');
				if(ptr != NULL) {
					unsigned length = strlen(ptr + 1);
					memmove(names[i], ptr + 1, length);
					names[i][length] = 0;
					names[i] = realloc(names[i], sizeof(char) * (length + 1));
				}
			}
		}

	return names;
}
