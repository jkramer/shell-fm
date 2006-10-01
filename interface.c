/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <openssl/md5.h>

#include "include/service.h"
#include "include/hash.h"
#include "include/interface.h"
#include "include/autoban.h"
#include "include/settings.h"
#include "include/http.h"
#include "include/split.h"

extern pid_t playfork;
extern unsigned discovery, record, paused, changeTime;

int fetchkey(unsigned);
void canon(int);
const char * meta(const char *, int);
void radioprompt(const char *);
void run(const char *);
void tag(struct hash);
void artistRadio();

struct hash track;

extern struct hash data;

void interface(int interactive) {
	if(interactive) {
		int key;
		char customkey[8] = { 0 };
		
		canon(0);
		fflush(stderr);
		if((key = fetchkey(1)) == -1)
			return;

		switch(key) {
			case 'l':
				if(playfork)
					puts(control("love") ? "Loved." : "Sorry, failed.");
				break;

			case 'B':
				if(playfork)
					puts(control("ban") ? "Banned." : "Sorry, failed.");
				break;

			case 'n':
				if(playfork)
					control("skip") || puts("Sorry, failed.");
				break;

			case 'Q':
				exit(EXIT_SUCCESS);

			case 'i':
				if(playfork) {
					puts(meta("Station:\t%s (%u)", !0));
					puts(meta("Track:\t\t\"%t\" by %a", !0));
					puts(meta("Album:\t\t%A", !0));
				}
				break;

			case 'r':
				radioprompt("radio url> ");
				break;

			case 'd':
				discovery = !discovery;
				if(setdiscover(discovery))
					printf("%s discovery mode.\n", discovery ? "Enabled" : "Disabled");
				else
					printf("Failed to %s discovery mode.", discovery ? "enable" : "disable");
				break;

			case 'A':
				printf(meta("Really ban all tracks by artist %a? [yN]", !0));
				fflush(stdout);
				if(fetchkey(5) != 'y')
					puts("\nAbort.");
				else if(autoban(value(& track, "artist"))) {
					printf("\n%s banned.\n", meta("%a", !0));
					control("ban");
				}
				fflush(stdout);
				break;

			case 'R':
				record = !record;
				if(control(record ? "rtp" : "nortp"))
					printf("%s RTP.\n", record ? "Enabled" : "Disabled");
				else
					printf("Sorry, failed to %s RTP.\n", record ? "enable" : "disable");
				break;

			case 'f':
				if(playfork)
					station(meta("lastfm://artist/%a/fans", 0));
				break;
				
			case 's':
				if(playfork)
					station(meta("lastfm://artist/%a/similar", 0));
				break;

			case 'p':
				if(playfork) {
					if(paused)
						kill(playfork, SIGCONT);
					else
						kill(playfork, SIGSTOP);
					paused = !paused;
				}
				break;

			case 'S':
				if(playfork)
					kill(playfork, SIGKILL);
				break;

			case 'T':
				if(playfork)
					tag(track);
				break;

			case 'a':
				artistRadio();
				break;

      case '?':
				puts("a = Play Custom Artist Radion");
        puts("A = Autoban Artist");
        puts("B = Ban Track");
        puts("d = Discovery Mode");
        puts("f = Fan Station");
        puts("i = Current Track Information");
        puts("l = Love Track");
        puts("n = Skip Track");
        puts("p = Pause Track");
        puts("Q = Quit Shell-FM");
        puts("R = Enable/Disable RTP");
        puts("r = change radio station");
        puts("S = Stop");
        puts("s = Similiar Artist");
        puts("T = Tag Track/Artist/Album");
        break;

			default:
				snprintf(customkey, sizeof(customkey), "key0x%02X", key & 0xFF);
				if(haskey(& rc, customkey))
					run(meta(value(& rc, customkey), 0));
		}
	}
}

void radioprompt(const char * prompt) {
	char * url;
	
	canon(!0);
	url = readline(prompt);
	canon(0);

	if(!url)
		return;

	if(strlen(url) > 1) {
		char * decoded = NULL;
		decode(url, & decoded);
		add_history(decoded);
		station(decoded);
		free(decoded);
	}

	free(url);
}

int fetchkey(unsigned nsec) {
	fd_set fdset;
	struct timeval tv;

	FD_ZERO(& fdset);
	FD_SET(fileno(stdin), & fdset);

	tv.tv_usec = 0;
	tv.tv_sec = nsec;

	if(select(fileno(stdin) + 1, & fdset, NULL, NULL, & tv) > 0)
		return fgetc(stdin);

	return -1;
}

void canon(int enable) {
	struct termios term;
	tcgetattr(fileno(stdin), & term);
	term.c_lflag = enable
		? term.c_lflag | ICANON | ECHO
		: term.c_lflag & ~(ICANON | ECHO);
	tcsetattr(fileno(stdin), TCSANOW, & term);
}

#define remn (sizeof(string) - length - 1)
const char * meta(const char * fmt, int colored) {
	static char string[4096];
	unsigned length = 0, x = 0;
	
	if(!fmt || !playfork)
		return NULL;
	
	memset(string, 0, sizeof(string));
	
	while(fmt[x] && remn > 0) {
		if(fmt[x] != 0x25)
			string[length++] = fmt[x++];
		else if(fmt[++x]) {
			const char * keys [] = {
				"aartist",
				"ttrack",
				"Aalbum",
				"dtrackduration",
				"sstation",
				"ustation_url",
				"Uartist_url",
				"Xalbum_url",
				"Ttrack_url",
				"Rremain"
			};

			register unsigned i = sizeof(keys) / sizeof(char *);
			
			while(i--)
				if(fmt[x] == keys[i][0]) {
					const char * val = value(& track, keys[i] + 1), * color = NULL;
					if(colored) {
						char colorkey[64] = { 0 };
						snprintf(colorkey, sizeof(colorkey), "%c-color", keys[i][0]);
						color = value(& rc, colorkey);
						if(color)
							length += snprintf(string + length, remn, "\x1B[%sm", color);
					}
					length = strlen(strncat(string, val ? val : "(unknown)", remn));
					if(color)
						length = strlen(strncat(string, "\x1B[0;37m", remn));
					break;
				}
			++x;
		}
	}

	return string;
}
#undef remn

void run(const char * cmd) {
	if(!fork()) {
		FILE * fd = popen(cmd, "r");
		if(!fd)
			exit(EXIT_FAILURE);
		else {
			char ch;
			
			while((ch = fgetc(fd)) != EOF)
				fputc(ch, stdout);

			fflush(stdout);
			_exit(pclose(fd));
		}
	}
}

void tag(struct hash data) {
	char key, * tagstring;

	fputs("Tag artist, album or track (or abort)? [aAtq]: ", stdout);
	fflush(stdout);

	while(!strchr("aAtq", (key = fetchkey(2))));

	if(key == 'q')
		return;

	canon(!0);
	tagstring = readline("Your tags, comma separated:\n>> ");
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


void artistRadio(void) {
	char * artists;

	fputs("Enter one or more artists, separated by commas.\n", stdout);

	canon(!0);
	artists = readline(">> ");
	canon(0);

	if(strlen(artists) > 0) {
		char * post, * encoded = NULL, ** resp;
		unsigned length = encode(artists, & encoded) + 11, x = 0;

		free(artists);
		post = calloc(length, sizeof(char));
		snprintf(post, length, "artists=%s\r\n", encoded);
		free(encoded);

		resp = fetch("http://www.last.fm/listen/", NULL, post, "Content-Type: application/x-www-form-urlencoded");
		free(post);

		if(resp) {
			while(resp[x]) {
				if(!resp[x + 1])
					station(resp[x]);
				free(resp[x++]);
			}

			free(resp);
		}
	}
}
