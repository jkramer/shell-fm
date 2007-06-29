
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <assert.h>
#include <time.h>

#include "hash.h"
#include "http.h"
#include "md5.h"


struct hash submission, * queue = NULL;
unsigned qlength = 0, submitting = 0;

int handshaked = 0;
pid_t subfork = 0;


int handshake(const char *, const char *);
void sliceq(unsigned);


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

	snprintf(timestamp, sizeof(timestamp), "%lu", time(NULL));
	snprintf(lastid, sizeof(lastid), "L%s", value(track, "lastfm:trackauth"));
	snprintf(duration, sizeof(duration), "%d", atoi(value(track, "duration")) / 1000);

	set(& post, "a", value(track, "creator"));
	set(& post, "t", value(track, "title"));
	set(& post, "i", timestamp);
	set(& post, "r", "");
	set(& post, "o", lastid);
	set(& post, "l", duration);
	set(& post, "b", value(track, "album"));
	set(& post, "n", "");
	set(& post, "m", "");


	memcpy(& queue[qlength++], & post, sizeof(struct hash));

	return !0;
}


void ratelast(const char * rating) {
	if(strchr("LBS", * rating) && qlength > 0)
		set(& queue[qlength - 1], "r", rating);
}


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


	if(!handshaked && !handshake(user, password)) {
		fputs("Handshake failed.\n", stderr);
		return 0;
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

	resp = fetch(value(& submission, "submissions"), NULL, body);

	if(resp) {
		unsigned i;

		if(resp[0]) {
			printf("DEBUG: RESPONSE=<%s>\n", resp[0]);
			if(!strncmp(resp[0], "OK", 2))
				retval = 0;
		}

		for(i = 0; resp[i] != NULL; ++i)
			free(resp[i]);

		free(resp);
	}

	free(body);

	if(retval)
		puts("Couldn't scrobble track(s).");

	exit(retval);
}


void sliceq(unsigned tracks) {
	unsigned i;

	if(tracks > qlength)
		tracks = qlength;

	for(i = 0; i < tracks; ++i)
		empty(& queue[i]);

	qlength -= tracks;

	if(qlength > 0)
		memmove(queue, & queue[tracks], sizeof(struct hash) * qlength);

	queue = realloc(queue, sizeof(struct hash) * qlength);
}


int handshake(const char * user, const char * password) {
	char temp[10 + 32 + 1], hex[32 + 1], ** resp;
	const char * url;
	const unsigned char * md5;
	int i, retval = 0;
	time_t timestamp = time(NULL);

	if(handshaked) {
		fputs("Empty in handshake.", stderr);
		empty(& submission);
	}

	memset(& submission, 0, sizeof(struct hash));
	handshaked = 0;

	assert(user != NULL);
	assert(password != NULL);

	snprintf(temp, sizeof(temp), "%s%lu", password, timestamp);
	md5 = MD5((unsigned char *) temp, strlen(temp));

	for(i = 0; i < 16; ++i)
		sprintf(& hex[2 * i], "%02x", md5[i]);

	url = makeurl(
		"http://post.audioscrobbler.com/" /* Base URL. */
		"?hs=true" /* Handshake? Yes! */
		"&p=%s" /* Protocol 1.2. */
		"&c=tst" /* Client ID (get a real one later). */
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

	resp = fetch(url, NULL, NULL);
	if(resp != NULL) {
		if(resp[0] != NULL && !strncmp("OK", resp[0], 2)) {
			handshaked = !0;
			set(& submission, "session", resp[1]);
			set(& submission, "now-playing", resp[2]);
			set(& submission, "submissions", resp[3]);

			retval = !0;
		}

		for(i = 0; resp[i] != NULL; ++i)
			free(resp[i]);

		free(resp);
	}

	return retval;
}


void subdead(int exitcode) {
	if(exitcode == 0)
		sliceq(submitting);

	subfork = 0;
	submitting = 0;
}
