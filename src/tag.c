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
#include <ctype.h>
#include <assert.h>

#include "settings.h"
#include "http.h"
#include "split.h"
#include "interface.h"
#include "completion.h"
#include "md5.h"
#include "feeds.h"

#include "readline.h"
#include "tag.h"

static char * oldtags(char, struct hash);
static char ** popular = NULL;

static int tagcomplete(char *, const unsigned, int);

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

	fputs("Tag (a)rtist, a(l)bum, (t)rack or (c)ancel?\n", stderr);

	while(!strchr("altc", (key = fetchkey(2))));

	if(key == 'c')
		return;

	if((popular = toptags(key, data))) {
		unsigned items = 0;
		while(popular[items])
			++items;
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

			case 'l':
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


static char * oldtags(char key, struct hash track) {
	unsigned length, x;
	char * tags = NULL, * url = calloc(512, sizeof(char)),
			 * user = NULL, * artist = NULL, * arg = NULL,
			 * file = NULL, ** resp;
	
	switch(key) {
		case 'a':
			file = "artisttags.xml";
			break;
		case 'l':
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

	if(key == 'l') {
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


void stripslashes(char * string) {
	unsigned x = 0;
	while(x < strlen(string)) {
		if(string[x] == 0x2F)
			strncpy(string + x, string + x + 1, strlen(string + x + 1));
		else
			++x;
	}
}


static int tagcomplete(char * line, const unsigned max, int changed) {
	unsigned length, nres = 0;
	int retval = 0;
	char * ptr = NULL;
	const char * match = NULL;

	assert(line != NULL);

	length = strlen(line);

	/* Remove spaces at the beginning of the string. */
	while(isspace(line[0])) {
		retval = !0;
		memmove(line, line + 1, strlen(line + 1));
		line[--length] = 0;
	}

	/* Remove spaces at the end of the string. */
	while(isspace(line[(length = strlen(line)) - 1])) {
		retval = !0;
		line[--length] = 0;
	}

	/* No need for tab completion if there are no popular tags. */
	if(!popular || !popular[0])
		return retval;

	/* Get pointer to the beginning of the last tag in tag string. */
	if((ptr = strrchr(line, ',')) == NULL)
		ptr = line;
	else
		++ptr;

	length = strlen(ptr);
	if(!length)
		changed = !0;

	/* Get next match in list of popular tags. */
	if((match = nextmatch(popular, changed ? ptr : NULL, & nres)) != NULL) {
		snprintf(ptr, max - (ptr - line) - 1, "%s%s", match, nres < 2 ? "," : "");
		retval = !0;
	}

	return retval;
}


/* http://ws.audioscrobbler.com/1.0/tag/toptags.xml */
