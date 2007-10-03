
#define _GNU_SOURCE

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "feeds.h"
#include "tag.h"

char ** neighbors(const char * user) {
	char * encoded = NULL, feed[128], ** names = NULL;
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

	names = cache(feed, "neighbors", 0);

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
	char * encoded = NULL, feed[128], ** names = NULL;
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

	names = cache(feed, "top-artists", 0);

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

char ** friends(const char * user) {
	char * encoded = NULL, feed[128];

	assert(user != NULL);
	encode(user, & encoded);

	memset(feed, (char) 0, sizeof(feed));
	snprintf(
		feed, sizeof(feed),
		"http://ws.audioscrobbler.com/1.0/user/%s/friends.txt",
		encoded
	);

	free(encoded);

	return cache(feed, "friends", 0);
}


char ** toptags(char key, struct hash track) {
	unsigned length, x, count, idx;
	char ** tags = NULL, url[256], * type = NULL, * artist = NULL,
		 ** resp;

	/* Get artist, album or track tags? */
	type = strchr("al", key) ? "artist" : "track";

	/* Prepare artist name for use in URL. */
	encode(value(& track, "creator"), & artist);
	stripslashes(artist);

	/* Prepare URL for album / artist tags. */
	memset(url, 0, sizeof(url));
	length = snprintf(
		url, sizeof(url),
		"http://ws.audioscrobbler.com/1.0/%s/%s/",
		type, artist
	);

	free(artist);

	/* Append title if we want track tags. */
	if(key == 't') {
		char * title = NULL;
		encode(value(& track, "title"), & title);
		stripslashes(title);
		length += snprintf(url + length, sizeof(url) - length, "%s/", title);
		free(title);
	}

	strncpy(url + length, "toptags.xml", sizeof(url) - length - 1);

	/* Fetch XML document. */
	if((resp = cache(url, "top-tags", 0)) == NULL)
		return NULL;

	/* Count tags in XML. */
	for(count = x = 0; resp[x]; ++x)
		if(strstr(resp[x], "<name>") != NULL)
			++count;

	tags = calloc(count + 1, sizeof(char *));
	tags[count] = NULL;

	/* Search tag names in XML document and copy them into our list. */
	for(x = 0, idx = 0; resp[x] && idx < count; ++x) {
		char * pbeg = strstr(resp[x], "<name>");
		if(pbeg) {
			char * pend = strstr(pbeg += 6, "</name>");
			if(pend)
				tags[idx++] = strndup(pbeg, pend - pbeg);
		}

		free(resp[x]);
	}

	free(resp);

	return tags;
}


char ** overalltags(void) {
	unsigned x, count = 0, idx;
	const char * url = "http://ws.audioscrobbler.com/1.0/tag/toptags.xml";
	char ** tags = NULL, ** resp;

	if((resp = cache(url, "overall-tags", 0)) == NULL)
		return NULL;

	for(x = 0; resp[x]; ++x)
		if(strstr(resp[x], "<tag name=\"") != NULL)
			++count;

	tags = calloc(count + 1, sizeof(char *));
	tags[count] = NULL;

	for(x = 0, idx = 0; resp[x]; ++x) {
		char * pbeg = strstr(resp[x], "<tag name=\""), * pend;
		if(pbeg) {
			pend = strstr(pbeg += 11, "\"");
			if(pend)
				tags[idx++] = strndup(pbeg, pend - pbeg);
		}

		free(resp[x]);
	}

	free(resp);

	return tags;
}
