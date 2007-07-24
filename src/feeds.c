
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"
#include "feeds.h"
#include "tag.h"

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

char ** friends(const char * user) {
	char * encoded = NULL, feed[512];

	assert(user != NULL);

	encode(user, & encoded);

	memset(feed, (char) 0, sizeof(feed));
	snprintf(
		feed, sizeof(feed),
		"http://ws.audioscrobbler.com/1.0/user/%s/friends.txt",
		encoded
	);

	free(encoded);

	return fetch(feed, NULL, NULL);
}


char ** toptags(char key, struct hash track) {
	unsigned length, x, count, idx;
	char ** tags = NULL, * url = calloc(512, sizeof(char)),
			 * type = NULL, * artist = NULL, * arg = NULL,
			 * file = NULL, ** resp;

	file = "toptags.xml";
	
	switch(key) {
		case 'l': /* album has not special tags */
		case 'a': /* artist tags */
			type = "artist";
			break;
		case 't':
		default:
			type = "track";
	}

	encode(value(& track, "creator"), & artist);
	stripslashes(artist);

	length = snprintf(
			url, 512, "http://ws.audioscrobbler.com/1.0/%s/%s/",
			type, artist);

	free(artist);

	if(key == 't') {
		encode(value(& track, "title"), & arg);
		stripslashes(arg);
		length += snprintf(url + length, 512 - length, "%s/", arg);
		free(arg);
	}

	strncpy(url + length, file, 512 - length);

	resp = fetch(url, NULL, NULL);
	free(url);

	if(!resp)
		return NULL;

	count = 0;
	for(x = 0; resp[x]; ++x) {
		char * pbeg = strstr(resp[x], "<name>");
		if(pbeg)
			++count;
	}

	tags = calloc(count + 1, sizeof(char *));

	for(x = 0, idx = 0; resp[x]; ++x) {
		char * pbeg = strstr(resp[x], "<name>"), * pend;
		if(pbeg) {
			pbeg += 6;
			pend = strstr(pbeg, "</name>");
			if(pend) {
				tags[idx] = malloc(pend - pbeg + 1);
				memcpy(tags[idx], pbeg, pend - pbeg);
				tags[idx][pend - pbeg] = 0;
				++idx;

				if(idx >= count)
					break;
			}
		}

		free(resp[x]);
	}

	free(resp);

	tags[idx] = NULL;

	return tags;
}


