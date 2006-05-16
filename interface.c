/*
	vim:syntax=c tabstop=2 shiftwidth=2

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

#include <readline/readline.h>
#include <readline/history.h>

#include "include/service.h"
#include "include/hash.h"
#include "include/interface.h"
#include "include/autoban.h"
#include "include/settings.h"

extern pid_t playfork;
extern unsigned skipped, discovery, chstation, record;

int fetchkey(unsigned);
void canon(int);
const char * meta(const char *, int);
void radioprompt(const char *);
void run(const char *);

struct hash track;

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
					printf("%s.\n", control("love") ? "Loved" : "Sorry, failed");
				break;

			case 'B':
				if(playfork)
					printf("%s.\n", control("ban") ? "Banned" : "Sorry, failed");
				break;

			case 'n':
				if(playfork)
					control("skip") || printf("Sorry, failed");
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
					printf("\nAbort.\n");
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
		add_history(url);
		station(url);
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
				"Ttrack_url"
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
			exit(pclose(fd));
		}
	}
}
