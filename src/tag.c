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
#include <readline/readline.h>

#include "settings.h"
#include "http.h"
#include "split.h"
#include "interface.h"
#include "completion.h"
#include "md5.h"

static char ** getPopularTags(char, struct hash);
static char * getExistingTags(char, struct hash);

static char** rlcompletion(const char *text, int start, int end);
static int rlstartup(void);

static char * current_tags = NULL;
static char ** popular_tags = NULL;

void stripslashes(char *);

void tag(struct hash data) {
	char key, * tagstring;
	unsigned tslen;
	rl_params_t save_rlp;

	if(!data.content)
		return;

	fputs("Tag artist, album or track (or abort)? [aAtq]\n", stdout);
	fflush(stdout);

	while(!strchr("aAtq", (key = fetchkey(2))));

	if(key == 'q')
		return;

	save_rl_params(& save_rlp);

	if((popular_tags = getPopularTags(key, data))) {
		rl_basic_word_break_characters = ",";
		rl_completer_word_break_characters = ",";
		rl_completion_append_character = ',';
		rl_attempted_completion_function = rlcompletion;
	}

	if((current_tags = getExistingTags(key, data)))
		rl_startup_hook = rlstartup;

	canon(!0);
	tagstring = readline("Your tags, comma separated:\n>> ");

	if(current_tags) {
		free(current_tags);
		current_tags = NULL;
	}

	if(popular_tags) {
		unsigned x;
		for(x = 0; popular_tags[x]; ++x)
			free(popular_tags[x]);
		free(popular_tags);
		popular_tags = NULL;
	}

	canon(0);
	restore_rl_params(&save_rlp);

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
		while (tagstring[tslen-1] == ',') {
			tagstring[--tslen] = 0;
		}

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

		if((resp = fetch(url, NULL, xml, NULL))) {
			for(x = 0; resp[x]; ++x)
				free(resp[x]);
			free(resp);
		}

		free(xml);
	}

	free(tagstring);
}

static char ** getPopularTags(char key, struct hash track) {
	unsigned length, x, tag_count, tag_idx;
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

	strncpy (url+length, file, 512-length);

	resp = fetch(url, NULL, NULL, NULL);
	free(url);

	if(!resp)
		return NULL;

	tag_count = 0;
	for (x=0; resp[x]; x++) {
		char * pbeg = strstr(resp[x], "<name>");
		if (pbeg)
			tag_count ++;
	}

	tags = calloc (tag_count+1, sizeof (char*));

	for(x = 0, tag_idx = 0; resp[x]; ++x) {
		char * pbeg = strstr(resp[x], "<name>"), * pend;
		if(pbeg) {
			pbeg += 6;
			pend = strstr(pbeg, "</name>");
			if(pend) {
				tags[tag_idx] = malloc (pend - pbeg + 1);
				memcpy (tags[tag_idx], pbeg, pend - pbeg);
				tags[tag_idx][pend - pbeg] = 0;
				tag_idx++;

				if (tag_idx >= tag_count)
					break;
			}
		}
		free(resp[x]);
	}
	free(resp);

	tags[tag_idx] = NULL;

	return tags;
}

static char * getExistingTags(char key, struct hash track) {
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

	resp = fetch(url, NULL, NULL, NULL);
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

static char * __tag_match_generator (const char *text, int state) {
	static int index, tlen;
	char *tag;

	if (!state) {
		/* first time */
		index = 0;
		tlen = strlen (text);
	}

	while ((tag = popular_tags[index])) {
		index ++;
		if (strncmp (tag, text, tlen) == 0)
			return strdup(tag);
	}

	return NULL;
}

static char** rlcompletion(const char *text, int start __attribute__((unused)), int end __attribute__((unused))) {
	char ** ret;

	ret = rl_completion_matches (text, __tag_match_generator);

	return ret;
}

static int rlstartup(void) {
	if(current_tags)
		rl_insert_text(current_tags);
	return 0;
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
