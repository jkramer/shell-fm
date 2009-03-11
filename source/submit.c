/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#include <assert.h>
#include <time.h>

#include "hash.h"
#include "http.h"
#include "md5.h"
#include "settings.h"
#include "split.h"
#include "getln.h"
#include "strary.h"

#include "submit.h"
#include "globals.h"

#define ERROR (strerror(errno))

struct hash submission, * queue = NULL;
unsigned qlength = 0, submitting = 0;

int handshaked = 0;
pid_t subfork = 0;

extern pid_t playfork;
extern struct hash data, rc, track;

static int handshake(const char *, const char *);
static void sliceq(unsigned);


/* Add a track to the scrobble queue. */
int enqueue(struct hash * track) {
	const char * keys [] = { "creator", "title", "album", "duration" };
	unsigned i;
	struct hash post;
	char timestamp[16], lastid[8], duration[8];

	assert(track != NULL);

	memset(& post, 0, sizeof(struct hash));

	for(i = 0; i < (sizeof(keys) / sizeof(char *)); ++i)
		if(!haskey(track, keys[i]))
			return 0;

	queue = realloc(queue, sizeof(struct hash) * (qlength + 1));
	assert(queue != NULL);

	snprintf(timestamp, sizeof(timestamp), "%lu", (unsigned long) time(NULL));
	snprintf(lastid, sizeof(lastid), "L%s", value(track, "lastfm:trackauth"));
	snprintf(duration, sizeof(duration), "%d", atoi(value(track, "duration")));

	set(& post, "a", value(track, "creator"));
	set(& post, "t", value(track, "title"));
	set(& post, "i", timestamp);
	set(& post, "r", value(track, "rating"));
	set(& post, "o", lastid);
	set(& post, "l", duration);
	set(& post, "b", value(track, "album"));
	set(& post, "n", "");
	set(& post, "m", "");

	memcpy(& queue[qlength++], & post, sizeof(struct hash));

	return !0;
}


/* Submit tracks from the queue. */
int submit(const char * user, const char * password) {
	char * body = NULL, ** resp;
	const unsigned stepsize = 1024 * sizeof(char);
	unsigned total = stepsize, ntrack;
	int retval = -1;


	if(!qlength || subfork > 0)
		return 0;

	submitting = qlength;
	subfork = fork();

	if(subfork != 0)
		return !0;

	playfork = 0;
	enable(QUIET);

	signal(SIGTERM, SIG_IGN);

	if(!handshaked && !handshake(user, password)) {
		fputs("Handshake failed.\n", stderr);
		exit(retval);
	}

	/* Prepare POST body. */
	body = malloc(stepsize);
	assert(body != NULL);
	memset(body, 0, stepsize);

	snprintf(body, stepsize, "s=%s", value(& submission, "session"));
	for(ntrack = 0; ntrack < qlength; ++ntrack) {
		unsigned pair;
		for(pair = 0; pair < queue[ntrack].size; ++pair) {
			char key[16], * encoded = NULL;
			unsigned length, bodysz = strlen(body);

			snprintf(
				key, sizeof(key), "%s[%d]",
				queue[ntrack].content[pair].key, ntrack
			);

			encode(queue[ntrack].content[pair].value, & encoded);
			length = strlen(key) + strlen(encoded) + 2;

			while(bodysz + length > total) {
				body = realloc(body, total + stepsize);
				assert(body != NULL);
				total += stepsize;
			}

			snprintf(body + bodysz, total - bodysz, "&%s=%s", key, encoded);
			free(encoded);
		}
	}

	sliceq(qlength);

	resp = fetch(value(& submission, "submissions"), NULL, body, NULL);

	if(resp) {
		if(resp[0] && !strncmp(resp[0], "OK", 2))
			retval = 0;

		purge(resp);
	}

	free(body);

	if(retval)
		puts("Couldn't scrobble track(s).");

	exit(retval);
}


/* Remove a number of tracks from the scrobble queue. */
static void sliceq(unsigned tracks) {
	unsigned i;

	if(tracks > qlength)
		tracks = qlength;

	for(i = 0; i < tracks; ++i)
		empty(& queue[i]);

	qlength -= tracks;

	if(qlength > 0) {
		memmove(queue, & queue[tracks], sizeof(struct hash) * qlength);
		queue = realloc(queue, sizeof(struct hash) * qlength);

		assert(queue != NULL);
	} else {
		free(queue);
		queue = NULL;
	}
}


/* Authenticate to the submission server. */
static int handshake(const char * user, const char * password) {
	char temp[10 + 32 + 1], hex[32 + 1], ** resp;
	const char * url;
	const unsigned char * md5;
	int i, retval = 0;
	time_t timestamp = time(NULL);

	if(handshaked)
		empty(& submission);

	memset(& submission, 0, sizeof(struct hash));
	handshaked = 0;

	assert(user != NULL);
	assert(password != NULL);

	snprintf(temp, sizeof(temp), "%s%lu", password, (unsigned long) timestamp);
	md5 = MD5((unsigned char *) temp, strlen(temp));

	for(i = 0; i < 16; ++i)
		sprintf(& hex[2 * i], "%02x", md5[i]);

	url = makeurl(
		"http://post.audioscrobbler.com/" /* Base URL. */
		"?hs=true" /* Handshake? Yes! */
		"&p=%s" /* Protocol 1.2. */
		"&c=sfm" /* Client ID (get a real one later). */
		"&v=%s" /* Client version. */
		"&u=%s" /* Last.FM user name. */
		"&t=%u" /* Timestamp. */
		"&a=%s", /* Authentication token. */

		"1.2",
		"1.0",
		user, /* Last.FM user name. */
		timestamp, /* UNIX timestamp. */
		hex /* The authentication MD5 token calculated above. */
	);

	if((resp = fetch(url, NULL, NULL, NULL)) != NULL) {
		if(resp[0] != NULL && !strncmp("OK", resp[0], 2)) {
			set(& submission, "session", resp[1]);
			set(& submission, "now-playing", resp[2]);
			set(& submission, "submissions", resp[3]);

			retval = handshaked = !0;
		}

		purge(resp);
	}

	return retval;
}


void subdead(int exitcode) {
	if(exitcode == 0)
		sliceq(submitting);

	subfork = 0;
	submitting = 0;
}


/* Write the tracks from the scrobble queue to a file. */
void dumpqueue(int overwrite) {
	const char * path = rcpath("scrobble-cache");
	if(path != NULL) {
		FILE * fd = fopen(path, overwrite ? "w" : "a+");
		if(fd != NULL) {
			unsigned n, x;
			for(n = 0; n < qlength; ++n) {
				const char keys [] = "atirolbnm";
				for(x = 0; x < sizeof(keys) - 1; ++x) {
					char key[2] = { keys[x], 0 }, * encoded = NULL;
					encode(value(& queue[n], key), & encoded);
					fprintf(fd, "%c=%s;", keys[x], encoded);
					free(encoded);
				}
				fputc(10, fd);
			}
			fclose(fd);
		} else {
			fprintf(stderr, "Couldn't open scrobble cache. %s.\n", ERROR);
		}
	} else {
		fprintf(stderr, "Can't find suitable scrobble queue path.\n");
	}

	sliceq(qlength);
}


void loadqueue(int overwrite) {
	const char * path = rcpath("scrobble-cache");

	if(overwrite)
		sliceq(qlength);

	if(path != NULL) {
		FILE * fd = fopen(path, "r");
		if(fd != NULL) {
			while(!feof(fd)) {
				char * line = NULL;
				unsigned size = 0;

				if(getln(& line, & size, fd) >= 2) {
					unsigned n = 0;
					char ** splt = split(line, ";\n", & n);
					struct hash track;

					memset(& track, 0, sizeof(struct hash));

					while(n--) {
						char key[2] = { 0 }, * value = NULL;
						sscanf(splt[n], "%c=", & key[0]);
						if(strchr("atirolbnm", key[0])) {
							decode(splt[n] + 2, & value);
							set(& track, key, value);
							free(value);
						}
						free(splt[n]);
					}

					free(splt);

					queue = realloc(queue, sizeof(struct hash) * (++qlength));
					assert(queue != NULL);
					memcpy(& queue[qlength - 1], & track, sizeof(struct hash));
				}

				if(line)
					free(line);
			}
			fclose(fd);
		}
	}
}
