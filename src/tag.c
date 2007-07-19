/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "http.h"
#include "split.h"
#include "interface.h"
#include "completion.h"
#include "md5.h"

#include "readline.h"

static char ** toptags(char, struct hash);
static char * oldtags(char, struct hash);

static char ** popular = NULL;

static void stripslashes(char *);

static int tagcomplete(char *, const unsigned);
static int tagcompare(const void *, const void *);

void tag(struct hash data) {
	char key, * tagstring;
	unsigned tslen;
	struct prompt setup = {
		.prompt = "Tags (comma separated): ",
		.line = NULL,
		.history = NULL,
		.callback = tagcomplete,
	};

	if(!data.content)
		return;

	fputs("Tag artist, album or track (or abort)? [aAtq]\n", stdout);
	fflush(stdout);

	while(!strchr("aAtq", (key = fetchkey(2))));

	if(key == 'q')
		return;

	if((popular = toptags(key, data))) {
		unsigned items = 0;
		while(popular[items])
			++items;

		qsort(popular, items, sizeof(char *), tagcompare);
	}

	setup.line = oldtags(key, data);

	tagstring = strdup(readline(& setup));

	if(setup.line) {
		free(setup.line);
		setup.line = NULL;
	}

	if(popular) {
		unsigned x;
		for(x = 0; popular[x]; ++x)
			free(popular[x]);
		free(popular);
		popular = NULL;
	}

	if(tagstring && (tslen = strlen(tagstring))) {
		unsigned nsplt = 0, postlength = 0, x = 0, xmllength;
		const unsigned char * md5;
		const char * token = NULL;
		char
			* post = NULL, * xml = NULL, * challenge = "Shell.FM",
			* xmlformat = NULL, ** splt,
			** resp, * url = "http://ws.audioscrobbler.com/1.0/rw/xmlrpc.php",
			md5hex[32 + 1] = { 0 }, tmp[32 + 8 + 1] = { 0 };

		/* remove trailing commas */
		while (tagstring[tslen-1] == ',')
			tagstring[--tslen] = 0;

		splt = split(tagstring, ",\n", & nsplt);

		switch(key) {
			case 'a':
				xmlformat =
					"<?xml version=\"1.0\"?>\n"
					"<methodCall>\n"
					"\t<methodName>tagArtist</methodName>\n"
					"\t<params>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><array><data>%s</data></array></value></param>\n"
					"\t\t<param><value><string>set</string></value></param>\n"
					"\t</params>\n"
					"</methodCall>\n";
				break;

			case 'A':
				token = value(& data, "album");
				xmlformat =
					"<?xml version=\"1.0\"?>\n"
					"<methodCall>\n"
					"\t<methodName>tagAlbum</methodName>\n"
					"\t<params>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><array><data>%s</data></array></value></param>\n"
					"\t\t<param><value><string>set</string></value></param>\n"
					"\t</params>\n"
					"</methodCall>\n";
				break;

			case 't':
				token = value(& data, "title");
				xmlformat =
					"<?xml version=\"1.0\"?>\n"
					"<methodCall>\n"
					"\t<methodName>tagTrack</methodName>\n"
					"\t<params>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><string>%s</string></value></param>\n"
					"\t\t<param><value><array><data>%s</data></array></value></param>\n"
					"\t\t<param><value><string>set</string></value></param>\n"
					"\t</params>\n"
					"</methodCall>\n";
				break;
		}

		while(x < nsplt) {
			unsigned taglength = strlen(splt[x]) + 33;
			post = realloc(post, postlength + taglength + 1);
			postlength += snprintf(post + postlength, taglength,
				"<value><string>%s</string></value>", splt[x]);
			free(splt[x++]);
		}

		free(splt);

		xmllength =
			strlen(xmlformat)
			+ strlen(value(& rc, "username"))
			+ strlen(challenge)
			+ sizeof(md5hex)
			+ strlen(value(& data, "creator"))
			+ (token ? strlen(token) : 0)
			+ postlength;

		/* generate password/challenge hash */
		snprintf(tmp, sizeof(tmp), "%s%s", value(& rc, "password"), challenge);
		md5 = MD5((unsigned char *) tmp, sizeof(tmp) - 1);
		for(x = 0; x < 16; ++x)
			sprintf(2 * x + md5hex, "%02x", md5[x]);

		xml = calloc(xmllength, sizeof(char));
		if(key != 'a')
			snprintf(
				xml, xmllength, xmlformat,
				value(& rc, "username"), /* username */
				challenge, /* challenge */
				md5hex, /* password/challenge hash */
				value(& data, "creator"), /* artist */
				token, /* album/track */
				post /* tags */
			);
		else
			snprintf(
				xml, xmllength, xmlformat,
				value(& rc, "username"), /* username */
				challenge, /* challenge */
				md5hex, /* password/challenge hash */
				value(& data, "creator"), /* artist */
				post /* tags */
			);

		free(post);

		if((resp = fetch(url, NULL, xml)) != NULL) {
			for(x = 0; resp[x]; ++x)
				free(resp[x]);

			free(resp);
		}

		free(xml);
	}

	free(tagstring);
}


static char ** toptags(char key, struct hash track) {
	unsigned length, x, count, idx;
	char ** tags = NULL, * url = calloc(512, sizeof(char)),
			 * type = NULL, * artist = NULL, * arg = NULL,
			 * file = NULL, ** resp;

	file = "toptags.xml";
	
	switch(key) {
		case 'A': /* album has not special tags */
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


static char * oldtags(char key, struct hash track) {
	unsigned length, x;
	char * tags = NULL, * url = calloc(512, sizeof(char)),
			 * user = NULL, * artist = NULL, * arg = NULL,
			 * file = NULL, ** resp;
	
	switch(key) {
		case 'a':
			file = "artisttags.xml";
			break;
		case 'A':
			file = "albumtags.xml";
			break;
		case 't':
		default:
			file = "tracktags.xml";
	}

	encode(value(& track, "creator"), & artist);
	stripslashes(artist);

	encode(value(& rc, "username"), & user);

	length = snprintf(
			url, 512, "http://ws.audioscrobbler.com/1.0/user/%s/%s?artist=%s",
			user, file, artist);

	free(user);
	free(artist);

	if(key == 'A') {
		encode(value(& track, "album"), & arg);
		stripslashes(arg);
		length += snprintf(url + length, 512 - length, "&album=%s", arg);
	} else if(key == 't') {
		encode(value(& track, "title"), & arg);
		stripslashes(arg);
		length += snprintf(url + length, 512 - length, "&track=%s", arg);
	}

	if(arg)
		free(arg);

	resp = fetch(url, NULL, NULL);
	free(url);

	if(!resp)
		return NULL;

	for(x = 0, length = 0; resp[x]; ++x) {
		char * pbeg = strstr(resp[x], "<name>"), * pend;
		if(pbeg) {
			pbeg += 6;
			pend = strstr(pbeg, "</name>");
			if(pend) {
				char * thistag = strndup(pbeg, pend - pbeg);
				unsigned nlength = strlen(thistag) + length;
				if(length)
					++nlength;
				tags = realloc(tags, nlength + 1);
				sprintf(tags + length, "%s%s", length ? "," : "", thistag);
				free(thistag);
				length = nlength;
			}
		}
		free(resp[x]);
	}
	free(resp);

	return tags;
}


static void stripslashes(char * string) {
	unsigned x = 0;
	while(x < strlen(string)) {
		if(string[x] == 0x2F)
			strncpy(string + x, string + x + 1, strlen(string + x + 1));
		else
			++x;
	}
}


static int tagcomplete(char * line, const unsigned max) {
	char * ptr;
	unsigned length, i;
	static int index = 0, lastsug = -1;
	static char last[256] = { 0, };

	if(!popular)
		return 0;

	if((ptr = strrchr(line, ',')) == NULL)
		ptr = line;
	else
		++ptr;

	if(lastsug < 0 || strcmp(popular[lastsug], ptr)) {
		index = 0;
		lastsug = -1;
		memset(last, 0, sizeof(0));
	}

	if(* last) {
		* ptr = 0;
		strcat(ptr, last);
	}

	length = strlen(ptr);

	if(length > 0) {
		int matches = 0;

		for(i = 0; popular[i]; ++i)
			if(!strncasecmp(ptr, popular[i], length))
				++matches;

		if(matches < 1)
			return 0;
		else if(matches > 1) {
			int match = 0;
			++index;
			if(index > matches)
				index = 1;

			if(lastsug < 0)
				strncpy(last, ptr, length);

			for(i = 0; popular[i]; ++i)
				if(!strncasecmp(popular[i], last, strlen(last))) {
					++match;
					if(match == index)
						break;
				}

			line[strlen(line) - strlen(last)] = 0;
			strncpy(ptr, popular[i], max - (unsigned) (ptr - line) - 1);
			lastsug = i;
		} else {
			for(i = 0; popular[i] && strncasecmp(ptr, popular[i], length); ++i);
			strncat(line, popular[i] + length, max - strlen(line));
			strncat(line, ",", max - strlen(line));
		}
	} else {
		unsigned width = 0;
		fputs("\n\nPopular tags:\n", stderr);
		for(i = 0; popular[i]; ++i) {
			width += strlen(popular[i]) + 2;
			fputs(popular[i], stderr);

			if(popular[i + 1]) {
				fputs(", ", stderr);

				if(width + strlen(popular[i + 1]) >= 60) {
					fputs("\n", stderr);
					width = 0;
				}
			} else
				fputs("\n", stderr);
		}

		fputs("\n", stderr);
	}

	return !0;
}


static int tagcompare(const void * one, const void * two) {
	return strcasecmp(* (char * const *) one, * (char * const *) two);
}
