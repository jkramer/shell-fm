
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <openssl/md5.h>

#include "include/settings.h"
#include "include/http.h"
#include "include/split.h"
#include "include/interface.h"

char * existingTags(char, struct hash);
int rlstartup(void);

char * rltags = NULL;

void tag(struct hash data) {
	char key, * tagstring;

	fputs("Tag artist, album or track (or abort)? [aAtq]\n", stdout);
	fflush(stdout);

	while(!strchr("aAtq", (key = fetchkey(2))));

	if(key == 'q')
		return;

	if((rltags = existingTags(key, data)))
		rl_startup_hook = rlstartup;
	canon(!0);
	tagstring = readline("Your tags, comma separated:\n>> ");
	if(rltags) {
		free(rltags);
		rltags = NULL;
	}
	rl_startup_hook = NULL;
	canon(0);

	if(tagstring && strlen(tagstring)) {
		unsigned nsplt = 0, postlength = 0, x = 0, xmllength;
		unsigned char * md5;
		const char * token = NULL;
		char
			* post = NULL, * xml = NULL, * challenge = "Shell.FM",
			* xmlformat = NULL, ** splt = split(tagstring, ",\n", & nsplt),
			** resp, * url = "http://ws.audioscrobbler.com/1.0/rw/xmlrpc.php",
			md5hex[32 + 1] = { 0 }, tmp[32 + 8 + 1] = { 0 };


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
				token = value(& data, "track");
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
			+ strlen(value(& data, "artist"))
			+ (token ? strlen(token) : 0)
			+ postlength;

		/* generate password/challenge hash */
		snprintf(tmp, sizeof(tmp), "%s%s", value(& rc, "password"), challenge);
		md5 = MD5((unsigned char *) tmp, sizeof(tmp) - 1, NULL);
		for(x = 0; x < 16; ++x)
			sprintf(2 * x + md5hex, "%02x", md5[x]);

		xml = calloc(xmllength, sizeof(char));
		if(key != 'a')
			snprintf(
				xml, xmllength, xmlformat,
				value(& rc, "username"), /* username */
				challenge, /* challenge */
				md5hex, /* password/challenge hash */
				value(& data, "artist"), /* artist */
				token, /* album/track */
				post /* tags */
			);
		else
			snprintf(
				xml, xmllength, xmlformat,
				value(& rc, "username"), /* username */
				challenge, /* challenge */
				md5hex, /* password/challenge hash */
				value(& data, "artist"), /* artist */
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

char * existingTags(char key, struct hash track) {
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

	encode(value(& track, "artist"), & artist);
	encode(value(& rc, "username"), & user);

	length = snprintf(
			url, 512, "http://wsdev.audioscrobbler.com/1.0/user/%s/%s?artist=%s",
			user, file, artist);

	free(user);
	free(artist);

	if(key == 'A') {
		encode(value(& track, "album"), & arg);
		length += snprintf(url + length, 512 - length, "&album=%s", arg);
	} else if(key == 't') {
		encode(value(& track, "track"), & arg);
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

int rlstartup(void) {
	if(rltags)
		rl_insert_text(rltags);
	return 0;
}
