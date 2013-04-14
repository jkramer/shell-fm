/*
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
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <sys/select.h>

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
#include "readline.h"
#include "recommend.h"
#include "util.h"
#include "tag.h"
#include "rest.h"

#include "globals.h"

extern time_t pausetime;

void unlinknp(void);

struct hash track;

char * shellescape(const char *);

void print_help(void);
void rate_command(const char *);

void handle_keyboard_input() {
	int key;
	char customkey[8] = { 0 }, * marked = NULL;

	fflush(stderr);
	if((key = fgetc(stdin)) == -1)
		return;

	if(key == 27) {
		int ch;
		while((ch = fgetc(stdin)) != -1 && !strchr("ABCDEFGHMPQRSZojmk~", ch));
		return;
	}

	snprintf(customkey, sizeof(customkey), "key0x%02X", key & 0xFF);
	if(haskey(& rc, customkey))
		run(meta(value(& rc, customkey), M_SHELLESC, & track));

	switch(key) {
		case 'l':
			puts(rate(RATING_LOVE) ? "Loved." : "Sorry, failed.");
			break;

		case 'U':
			puts(rate(RATING_UNLOVE) ? "Unloved." : "Sorry, failed.");
			break;

		case 'B':
			puts(rate(RATING_BAN) ? "Banned." : "Sorry, failed.");
			fflush(stdout);
			enable(INTERRUPTED);
			kill(playfork, SIGUSR1);
			break;

		case 'n':
			skip();
			break;

		case 'q':
			if(haskey(& rc, "delay-change")) {
				delayquit = !delayquit;
				if(delayquit)
					fputs("Going to quit soon.\n", stderr);
				else
					fputs("Delayed quit cancelled.\n", stderr);
			}
			break;

		case 'Q':
			quit();

		case 'i':
			if(playfork) {
				const char * path = rcpath("i-template");
				if(path && !access(path, R_OK)) {
					char ** template = slurp(path);
					if(template != NULL) {
						unsigned n = 0;
						while(template[n]) {
							puts(meta(template[n], M_COLORED, & track));
							free(template[n++]);
						}
						free(template);
					}
				}
				else {
					puts(meta("Track:    \"%t\" (%T)", M_COLORED, & track));
					puts(meta("Artist:   \"%a\" (%A)", M_COLORED, & track));
					puts(meta("Album:    \"%l\" (%L)", M_COLORED, & track));
					puts(meta("Station:  %s", M_COLORED, & track));
				}
			}
			break;

		case 'r':
			radioprompt("radio url> ");
			break;

		case 'd':
			toggle(DISCOVERY);
			printf("Discovery mode %s.\n", enabled(DISCOVERY) ? "enabled" : "disabled");
			if(playfork) {
				printf(
					"%u track(s) left to play/skip until change comes into effect.\n",
					playlist.left
				);
			}
			break;

		case 'A':
			printf("%s", meta("Really ban all tracks by artist %a? [yN]", M_COLORED, & track));
			fflush(stdout);
			if(fetchkey(5000000) != 'y')
				puts("\nAbort.");
			else if(autoban(value(& track, "creator"))) {
				printf("\n%s banned.\n", meta("%a", M_COLORED, & track));
				rate(RATING_BAN);
			}
			fflush(stdout);
			break;

		case 'a':
			puts("Adding tracks to playlists currently not implemented.");
			break;

		case 'P':
			toggle(RTP);
			printf("%s RTP.\n", enabled(RTP) ? "Enabled" : "Disabled");
			break;

		case 'f':
			if(playfork) {
				station(meta("lastfm://artist/%a/fans", 0, & track));
			}
			break;

		case 's':
			if(playfork) {
				station(meta("lastfm://artist/%a/similarartists", 0, & track));
			}
			break;

		case 'h':
			printmarks();
			break;

		case 'H':
			if(playfork && current_station) {
				puts("Enter a key for the bookmark.");
				fflush(stdout);
				key = fetchkey(5000000);
				setmark(current_station, key);
			}
			break;

		case 'S':
			if(playfork) {
				enable(STOPPED);
				kill(playfork, SIGUSR1);
			}
			break;

		case 'T':
			if(playfork)
				tag(track);
			break;

		case 'p':
			if(playfork) {
				if(pausetime) {
					kill(playfork, SIGCONT);
				}
				else {
					time(& pausetime);
					kill(playfork, SIGSTOP);
				}
			}
			break;

		case 'R':
			if(playfork) {
				recommend(track);
			}
			break;

		case '+':
		case '-':
			if(key == '+')
				volume_up();
			else
				volume_down();

			if(haskey(& rc, "volume-update")) {
				puts(meta(value(& rc, "volume-update"), M_COLORED, & track));
				fflush(stdout);
			}

			break;

		case 'm':
			mute();
			break;

		case 'u':
			preview(playlist);
			break;

		case 'E':
			expand(& playlist);
			break;

		case '?':
			print_help();
			break;

		case 'g':
			key = fetchkey(1000000);
			if((marked = getmark(key))) {
				station(marked);
				free(marked);
			}
			else {
				puts("Bookmark not defined.");
			}
			break;

		case 'C': /* Reload configuration. */
			settings(rcpath("shell-fm.rc"), 0);
			break;
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


#define remn (sizeof(string) - length - 1)
const char * meta(const char * fmt, int flags, struct hash * track) {
	static char string[4096];
	unsigned length = 0, x = 0;

	/* Switch off coloring when in batch mode */
	if(batch)
		flags &= ~M_COLORED;

	if(!fmt)
		return NULL;

	memset(string, 0, sizeof(string));

	while(fmt[x] && remn > 0) {
		if(fmt[x] != '%')
			string[length++] = fmt[x++];
		else if(fmt[++x]) {
			if(fmt[x] == '%') {
				string[length++] = fmt[x++];
			}
			else if(fmt[x] == '~' && getenv("HOME")) {
				length = strlen(strncat(string, getenv("HOME"), remn));
				x++;
			}
			else {
				char * val = NULL;
				const char * track_key = NULL;
				char tagging_item = 0;
				const char * color = NULL;
				// vars for internally generated values
				int remain;
				int duration;
				char calculated[10];

				switch(fmt[x]) {
					case 'a':
						track_key = "creator";
						break;
					case 't':
						track_key = "title";
						break;
					case 'l':
						track_key = "album";
						break;
					case 'A':
						track_key = "artistpage";
						break;
					case 'T':
						track_key = "trackpage";
						break;
					case 'L':
						track_key = "albumpage";
						break;
					case 'd':
						track_key = "duration";
						break;
					case 'f':
						duration = atoi(value(track, "duration")); // duration in sec
						snprintf(
							calculated,
							sizeof(calculated),
							"%d:%02d",
							(duration / 60), (duration % 60));
						val = strdup(calculated);
						break;
					case 'p':
						val = strdup(PLAYBACK_STATUS);
						break;
					case 's':
						track_key = "station";
						break;
					case 'S':
						track_key = "stationURL";
						break;
					case 'R':
						track_key = "remain";
						break;
                	case 'r':   // remaining time, formatted as min:sec
			            remain = atoi(value(track, "remain"));
					    snprintf(
    					    calculated,
    					    sizeof(calculated),
					        "%c%02d:%02d",
					        remain < 0 ? '-' : ' ',
					        (remain >= 0) ? (remain / 60) : (-remain / 60),
					        (remain >= 0) ? (remain % 60) : (-remain % 60));
					    val = strdup(calculated);
					    break;

                	case 'v': // volume percentage
					    snprintf(
					        calculated,
					        sizeof(calculated),
					        "%d%%",
					        ((volume * 100 / MAX_VOLUME * 100) / 100));
					    val = strdup(calculated);
					    break;

					case 'b': // absolute volume
						snprintf(
							calculated,
							sizeof(calculated),
							"%d",
							volume
						);
					    val = strdup(calculated);
						break;

					case 'V':
						track_key = "rating";
						break;
					case 'I':
						track_key = "image";
						break;
					case 'Z':
						// artist tags
						tagging_item = 'a';
						break;
					case 'D':
						// album tags
						tagging_item = 'l';
						break;
					case 'z':
						// track tags
						tagging_item = 't';
						break;
					default:
						val = strdup("");
						break;
				}

				if(track_key) {
					val = strdup(value(track, track_key));
				}
				else if(tagging_item) {
					val = NULL; // TODO oldtags(tagging_item, * track);
					if(!val) {
						val = strdup("");
					}
				}

				if(flags & M_COLORED) {
					char colorkey[64] = { 0 };
					snprintf(colorkey, sizeof(colorkey), "%c-color", fmt[x]);
					color = value(& rc, colorkey);

					if(strlen(color)) {
						/* Strip leading spaces from end of color (Author: Ondrej Novy) */
						char * color_st = strdup(color);
						size_t len = strlen(color_st) - 1;

						assert(color_st != NULL);

						while(isspace(color_st[len]) && len > 0) {
							color_st[len] = 0;
							len--;
						}
						length += snprintf(string + length, remn, "\x1B[%sm", color_st);
						free(color_st);
					}
				}

				if((flags & M_RELAXPATH) && val) {
					unsigned n;
					size_t l = strlen(val);

					for(n = 0; n < l; ++n) {
						if(val[n] == '/')
							val[n] = '|';
					}
				}

				if(flags & M_SHELLESC) {
					char * escaped = shellescape(val);
					free(val);
					val = escaped;
				}

				length = strlen(strncat(string, val ? val : "(unknown)", remn));

				free(val);

				if(color)
					length = strlen(strncat(string, "\x1B[0m", remn));

				++x;
			}
		}
	}

	return string;
}
#undef remn


void run(const char * cmd) {
	if(!fork()) {
        _exit(system(cmd));
	}
}


int rate(int rating) {
	if(playfork) {
		struct hash p = { 0, NULL };
		const char * method, * error;
		char * response, full_method[32];

		switch(rating) {
			case RATING_LOVE:
				method = "love";
				break;

			case RATING_BAN:
				kill(playfork, SIGUSR1);
				enable(INTERRUPTED);
				method = "ban";
				break;

			case RATING_UNLOVE:
				method = "unlove";
				break;

			case RATING_UNBAN:
				method = "unban";
				break;

			default:
				return 0;
		}

		set(& p, "track", value(& track, "title"));
		set(& p, "artist", value(& track, "creator"));

		snprintf(full_method, sizeof(full_method), "track.%s", method);

		response = rest(full_method, & p);
		if(!response)
			return 0;

		error = error_message(response);

		free(response);
		empty(& p);

		if(error != NULL) {
			fprintf(stderr, "Failed to %s track. %s.\n", method, error);
		}
		else {
			char * rating_code[RATING_MAX];

			rating_code[RATING_LOVE] = "L";
			rating_code[RATING_BAN] = "B";
			rating_code[RATING_UNLOVE] = "U";
			rating_code[RATING_UNBAN] = "X"; // Banned tracks won't be played, so this actually can't happen.

			rate_command(rating_code[rating]);

			return 1;
		}
	}

	return 0;
}


void skip(void) {
	if(playfork) {
		enable(INTERRUPTED);
		kill(playfork, SIGUSR1);
		rate_command("S");
	}
}


void rate_command(const char * rating) {
	set(& track, "rating", rating);

	if(haskey(& rc, "rate-cmd"))
		run(meta(value(& rc, "rate-cmd"), M_SHELLESC, & track));
}


char * shellescape(const char * string) {
	char * escaped;
	unsigned length = 0, n, size;

	assert(string != NULL);

	size = strlen(string) * 2 + 1;
	/*
	 * Be sure to still escape the empty string. I.e., ./blah a b
	 * is different from ./blah a "" b.
	 */
	if (size <= 1)
	  size += 2;

	escaped = malloc(size);
	memset(escaped, 0, size);

	assert(string != NULL);

	for(n = 0; n < strlen(string); ++n) {
		if(!isalnum(string[n]))
			escaped[length++] = '\\';

		escaped[length++] = string[n];
		escaped[length] = 0;
	}

	/* escape the empty string... */
	if (!length)
		memcpy(escaped, "\"\"", 2);

	return escaped;
}


void quit() {
	unlinknp();
	exit(EXIT_SUCCESS);
}

void unlinknp(void) {
	/* Remove now-playing file. */
	if(haskey(& rc, "np-file")) {
		const char * np = value(& rc, "np-file");
		if(np != NULL)
			unlink(np);
	}
}

void print_help(void) {
	unsigned i, custom = 0;

	fputs(
		"a = add the track to the playlist | A = autoban artist\n"
		"B = ban Track                     | d = discovery mode\n"
		"E = manually expand playlist      | f = fan Station\n"
		"h = list bookmarks                | H = bookmark current radio\n"
		"i = current track information     | l = love track\n"
		"n = skip track                    | p = pause\n"
		"P = enable/disable RTP            | Q = quit\n"
		"r = change radio station          | R = recommend track/artist/album\n"
		"S = stop                          | s = similiar artist\n"
		"T = tag track/artist/album        | u = show upcoming tracks in playlist\n"
		"U = unlove track                  | + = increase volume\n"
		"- = decrease volume               | C = reload configuration\n"
		"m = mute/unmute                   | g = goto bookmark\n",
		stderr
	);

	for(i = 0; i < rc.size; ++i) {
		struct pair * p = & rc.content[i];
		unsigned key;

		if(sscanf(p->key, "key%x", & key) == 1) {
			const char * command = value(& rc, p->key);

			if(!custom++)
				fputs("\n", stderr);

			fprintf(stderr, "%c = %s\n", key, command);
		}
	}
}


int volume_up() {
	return set_volume(volume + 1);
}

int volume_down() {
	return set_volume(volume - 1);
}

void mute() {
  if(muted) {
    set_volume(saved_volume);
    muted = 0;
  } else {
    saved_volume = volume;
    set_volume(0);
    muted = 1;
  }
}

int set_volume(int new_volume) {
	char c;

	volume = new_volume;

	if(new_volume > MAX_VOLUME)
		volume = MAX_VOLUME;
	else if(new_volume < 0)
		volume = 0;
	else
		volume = new_volume;

	c = (char) volume;

	if(playpipe != 0)
		write(playpipe, & c, 1);
	return volume;
}
