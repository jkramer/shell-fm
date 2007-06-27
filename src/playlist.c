/*
	Shell.FM - service.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>

#include "hash.h"
#include "http.h"
#include "settings.h"
#include "service.h"

#include "playlist.h"

extern struct hash data;
extern int discovery;

void freenode(struct tracknode **);

int expand(struct playlist * list) {
	char url[512], ** response, * xml = NULL;
	unsigned n, length = 0;
	const char * fmt =
		"http://ws.audioscrobbler.com/radio/xspf.php?sk=%s&discovery=%d&desktop=0";

	assert(list != NULL);

	memset(url, 0, sizeof(url));
	snprintf(url, sizeof(url), fmt, value(& data, "session"), discovery);

	if((response = fetch(url, NULL, NULL))) {
		int retval;

		for(n = 0; response[n]; ++n) {
			xml = realloc(xml, sizeof(char) * (length + strlen(response[n]) + 1));
			strcpy(xml + length, response[n]);
			length += strlen(response[n]);
			xml[length] = 0;
		}

		retval = parsexspf(list, xml);
		free(xml);
		return retval;
	}

	return 0;
}

int parsexspf(struct playlist * list, const char * xml) {
	char * ptr;
	unsigned i, tracks = 0;

	assert(list != NULL);
	assert(xml != NULL);

	if((ptr = strcasestr(xml, "<title>")) != NULL) {
		char * track;
		list->title = strndup(ptr + 7, strcasestr(xml, "</title>") - ptr - 7);

		while((track = strcasestr(ptr, "<track>")) != NULL) {
			struct tracknode * node = NULL;
			char * next = strcasestr(track + 7, "<track>");
			const char * tags [] = {
				"location", "title", "album", "creator", "duration",
				"lastfm:trackauth",
			};

			if(next)
				* (next - 1) = 0;

			node = malloc(sizeof(struct tracknode));
			assert(node != NULL);

			memset(node, 0, sizeof(struct tracknode));

			for(i = 0; i < (sizeof(tags) / sizeof(char *)); ++i) {
				char beg[32] = { 0 }, end[32] = { 0 };

				sprintf(beg, "<%s>", tags[i]);
				sprintf(end, "</%s>", tags[i]);

				if((ptr = strcasestr(track, beg)) != NULL) {
					char * text = strndup(
						ptr + strlen(beg),
						(strcasestr(ptr, end)) - (ptr + strlen(beg))
					);

					assert(text != NULL);
					set(& node->track, tags[i], text);
					free(text);
				}
			}

			set(& node->track, "station", list->title);
			push(list, node);

			++tracks;

			if(!next)
				break;

			ptr = next;
		}

		return !0;
	}

	return 0;
}


void freelist(struct playlist * list) {
	if(list->title) {
		free(list->title);
		list->title = NULL;
	}

	while(list->track)
		shift(list);

}


void push(struct playlist * list, struct tracknode * node) {
	if(!list->track)
		list->track = node;
	else {
		struct tracknode * last = list->track;
		while(last->next != NULL)
			last = last->next;
		last->next = node;
	}

	++(list->left);
}

void shift(struct playlist * list) {
	if(list->track) {
		struct tracknode * node = list->track;
		list->track = node->next;
		empty(& node->track);
		free(node);
		--(list->left);
	}
}
