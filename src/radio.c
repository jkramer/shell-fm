/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab

	Shell.FM - interface.c
	Copyright (C) 2006 by Jonas Kramer
	Copyright (C) 2006 by Bart Trojanowski <bart@jukie.net>

	Published under the terms of the GNU General Public License (GPLv2).
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

#include "compatibility.h"
#include "service.h"
#include "interface.h"
#include "http.h"
#include "settings.h"
#include "completion.h"
#include "strary.h"
#include "md5.h"

#include "radio.h"

#include "readline.h"
#include "history.h"

static int radiocomplete(char *, const unsigned);


/*
 * This function is called to change the station
 * the user is prompted for the new radio station.
 *
 * Something like:
 *
 * lastfm://user/BartTrojanowski/loved
 * lastfm://user/BartTrojanowski/personal
 * lastfm://user/BartTrojanowski/neighbour
 * lastfm://user/BartTrojanowski/recommended/100
 * lastfm://usertags/BartTrojanowski/trance
 * lastfm://artist/QED/similarartists
 * lastfm://artist/QED/fans
 * lastfm://globaltags/goa
 * lastfm://globaltags/classical,miles davis,whatever
 * lastfm://multipleartists/QED,Chicane
 * lastfm://play/tracks/########[,#####, ...]
 */
void radioprompt(const char * prompt) {
	char * url, * decoded = NULL;
	unsigned i = 0;

	struct prompt setup = {
		.prompt = prompt,
		.line = NULL,
		.history = slurp(rcpath("radio-history")),
		.callback = radiocomplete,
	};

	url = readline(& setup);

	if(setup.history) {
		while(setup.history[i])
			free(setup.history[i++]);
		free(setup.history);
	}

	if(strlen(url)) {
		decode(url, & decoded);
		station(decoded);
		free(decoded);
	}
}


int radiocomplete(char * line, const unsigned max) {
	unsigned length = strlen(line), nsplt = 0;
	char ** splt;

	if(!strncasecmp(line, "lastfm://", 9)) {
		memmove(line, line + 9, 9);
		memset(line + 9, 0, max - (length -= 9));
	}

	splt = split(line, "/", & nsplt);
	
	if(!nsplt) {
		free(splt);
		return 0;
	}

	if(nsplt > 0) {
	}

	return !0;
}
