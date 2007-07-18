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
#include <stdarg.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "service.h"
#include "hash.h"
#include "interface.h"
#include "autoban.h"
#include "settings.h"
#include "http.h"
#include "split.h"
#include "bookmark.h"
#include "radio.h"
#include "md5.h"
#include "submit.h"

extern pid_t playfork;
extern int discovery, record, changeTime, stopped;
extern char * currentStation;
extern time_t pausetime;

int fetchkey(unsigned);
void canon(int);
const char * meta(const char *, int);
void run(const char *);
void tag(struct hash);
void artistRadio();
void rate(const char *);

struct hash track;

extern struct hash data;
extern struct playlist playlist;


void interface(int interactive) {
	if(interactive) {
		int key;
		char customkey[8] = { 0 }, * marked = NULL;
		
		canon(0);
		fflush(stderr);
		if((key = fetchkey(1000000)) == -1)
			return;

		if(key == 27) {
			char ch;
			while((ch = fetchkey(100000)) != -1 && !strchr("ABCDEFGHMPQRSZojmk~", ch));
			return;
		}

		switch(key) {
			case 'l':
				if(playfork) {
					rate("L");
					puts(control("love") ? "Loved." : "Sorry, failed.");
				}
				break;

			case 'B':
				if(playfork) {
					rate("B");
					puts(control("ban") ? "Banned." : "Sorry, failed.");
				}
				break;

			case 'n':
				if(playfork) {
					rate("S");
					kill(playfork, SIGKILL);
				}
				break;

			case 'Q':
				unlink(rcpath("session"));
				exit(EXIT_SUCCESS);

			case 'i':
				if(playfork) {
					puts(meta("Track:    \"%t\"", !0));
					puts(meta("Artist:   \"%a\"", !0));
					puts(meta("Album:    \"%A\"", !0));
					puts(meta("Station:  %s", !0));
				}
				break;

			case 'r':
				radioprompt("radio url> ");
				break;

			case 'd':
				discovery = !discovery;
				printf("Discovery mode %s.\n", discovery ? "enabled" : "disabled");
				if(playfork) {
					printf(
						"%d track(s) left to play/skip until change comes into affect.\n",
						playlist.left
					);
				}
				break;

			case 'A':
				printf(meta("Really ban all tracks by artist %a? [yN]", !0));
				fflush(stdout);
				if(fetchkey(5000000) != 'y')
					puts("\nAbort.");
				else if(autoban(value(& track, "creator"))) {
					printf("\n%s banned.\n", meta("%a", !0));
					control("ban");
				}
				fflush(stdout);
				break;

			case 'R':
				record = !record;
				printf("%s RTP.\n", record ? "Enabled" : "Disabled");
				break;

			case 'f':
				if(playfork)
					station(meta("lastfm://artist/%a/fans", 0));
				break;
				
			case 's':
				if(playfork)
					station(meta("lastfm://artist/%a/similarartists", 0));
				break;

			case 'h':
				printmarks();
				break;

			case 'H':
				if(playfork && currentStation) {
					puts("What number do you want to bookmark this stream as? [0-9]");
					key = fetchkey(5000000);
					setmark(currentStation, key - 0x30);
				}
				break;

			case 'S':
				if(playfork) {
					stopped = !0;
					kill(playfork, SIGKILL);
				}
				break;

			case 'T':
				if(playfork)
					tag(track);
				break;

			case 'p':
				if(playfork)
					kill(playfork, pausetime ? SIGCONT : SIGSTOP);
				break;

      case '?':
        puts("A = Autoban Artist");
        puts("B = Ban Track");
        puts("d = Discovery Mode");
        puts("f = Fan Station");
				puts("h = List Bookmarks");
				puts("H = Bookmark Current Radio");
        puts("i = Current Track Information");
        puts("l = Love Track");
        puts("n = Skip Track");
        puts("Q = Quit Shell-FM");
        puts("R = Enable/Disable RTP");
        puts("r = change radio station");
        puts("S = Stop");
        puts("s = Similiar Artist");
        puts("T = Tag Track/Artist/Album");
        break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if((marked = getmark(key - 0x30))) {
					station(marked);
					free(marked);
				} else {
					puts("Bookmark not defined.");
				}
				break;

			default:
				snprintf(customkey, sizeof(customkey), "key0x%02X", key & 0xFF);
				if(haskey(& rc, customkey))
					run(meta(value(& rc, customkey), 0));
		}
	}
}

int fetchkey(unsigned nsec) {
	fd_set fdset;
	struct timeval tv;

	FD_ZERO(& fdset);
	FD_SET(fileno(stdin), & fdset);

	tv.tv_usec = nsec % 1000000;
	tv.tv_sec = nsec / 1000000;

	if(select(fileno(stdin) + 1, & fdset, NULL, NULL, & tv) > 0) {
		char ch = -1;
		if(read(fileno(stdin), & ch, sizeof(char)) == sizeof(char))
			return ch;
	}

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
	
	if(!fmt)
		return NULL;
	
	memset(string, 0, sizeof(string));
	
	while(fmt[x] && remn > 0) {
		if(fmt[x] != 0x25)
			string[length++] = fmt[x++];
		else if(fmt[++x]) {
			const char * keys [] = {
				"acreator",
				"ttitle",
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
						if(color) {
							// Strip leading spaces from end of color (Author: Ondrej Novy)
							char * color_st = strdup(color);
							size_t len = strlen(color_st) - 1;
							while (isspace(color_st[len]) && len > 0) {
								color_st[len] = 0;
								len--;
							}
							length += snprintf(string + length, remn, "\x1B[%sm", color_st);
							free(color_st);
						}
					}
					length = strlen(strncat(string, val ? val : "(unknown)", remn));
					if(color) {
						length = strlen(strncat(string, "\x1B[0m", remn));
					}
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


void rate(const char * rating) {
	if(rating != NULL)
		set(& track, "rating", rating);
}
