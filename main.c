/*
	vim:syntax=c tabstop=2 shiftwidth=2
	
	Shell.FM - main.c
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <time.h>
#include <readline/history.h>

#include "include/hash.h"
#include "include/service.h"
#include "include/interface.h"
#include "include/settings.h"
#include "include/autoban.h"
#include "include/version.h"

#define PATH_MAX 4096

extern struct hash data, track;
extern pid_t playfork;

void cleanup(void);
void killchild(int);
void songchanged(int);
void dying(int);

/* globals */
unsigned
	changed = 0 /* true if song changed */,
	skipped = 0 /* true if last song couldn't finish (skipped or banned) */,
	lastchange = 0 /* UNIX timestamp of the last songchange */,
	discovery = 0 /* discovery mode on/off switch */,
	chstation = 0 /* true if station changed */,
	record = !0 /* record tracks to profile */
	;

int main(int argc, char ** argv) {
	puts("Shell.FM v" VERSION ", written 2006 by Jonas Kramer");
	puts("Published under the terms of the GNU General Public License (GPL)\n");
	
	if(argc > 2) {
		fprintf(stderr, "usage: %s [lastfm://...]\n", * argv);
		exit(EXIT_FAILURE);
	}

	settings(rcpath("shell-fm.rc"), !0);

	if(!haskey(& rc, "password")) {
		char * password;
		if(!haskey(& rc, "username")) {
			char username[256] = { 0 };
			printf("Login: ");
			
			if(!scanf("%255s", username))
				exit(EXIT_SUCCESS);

			set(& rc, "username", username);
		}
		
		if(!(password = getpass("Password: ")))
			exit(EXIT_FAILURE);

		set(& rc, "password", password);
	}


	memset(& data, 0, sizeof(struct hash));
	memset(& track, 0, sizeof(struct hash));
	
	atexit(cleanup);
	signal(SIGCHLD, killchild);
	signal(SIGUSR1, songchanged);
	signal(SIGQUIT, dying);
	signal(SIGINT, dying);

	if(!handshake(value(& rc, "username"), value(& rc, "password")))
		exit(EXIT_FAILURE);

	if(argc == 2)
		station(argv[1]);
	else if(haskey(& rc, "default-radio"))
		station(value(& rc, "default-radio"));

	using_history();
	read_history(rcpath("radio-history"));

	while(!0) {
		if(changed) {
			char * last = strdup(meta("%a %t", 0));
			unsigned count = 0;

			if(last) {
				while(!strcmp(last, meta("%a %t", 0))) {
					if(count)
						fprintf(stderr, "No new data.\n");
					if(!update(& track)) {
						++count;
						fprintf(stderr, "Failed to fetch.\n");
					}
				}
				free(last);
			}

			if(banned(meta("%a", 0))) {
				const char * msg = meta(control("ban") ?
						"\"%t\" by %a auto-banned." : "Failed to auto-ban \"%t\" by %a.", !0);

				puts(msg);
				changed = 0;

				continue;
			}
			
			if(chstation) {
				printf("Receiving %s.\n", meta("%s", !0)); 
				chstation = 0;
			}

			printf("%s\n", meta("Now playing \"%t\" by %a.", !0));
			changed = 0;

			if(haskey(& rc, "np-file") && haskey(& rc, "np-file-format")) {
				signed np;
				const char
					* file = value(& rc, "np-file"),
					* fmt = value(& rc, "np-file-format");

				unlink(file);
				if(-1 != (np = open(file, O_WRONLY | O_CREAT, 0600))) {
					const char * output = meta(fmt, 0);
					if(output)
						write(np, output, strlen(output));
					close(np);
				}
			}

			if(haskey(& rc, "np-cmd") && !fork())
				system(meta(value(& rc, "np-cmd"), 0));
		}

		interface(!0);
	}
	
	return 0;
}

/* Clean up before exiting. Called by atexit(3). */
void cleanup(void) {
	write_history(rcpath("radio-history"));
	
	empty(& data);
	empty(& rc);

	if(playfork)
		kill(playfork, SIGTERM);
}

/* Called when play process died. */
void killchild(int sig) {
	if(sig == SIGCHLD) {
		if(wait(NULL) == playfork) {
			playfork = 0;
			lastchange = 0;
		}
	}
}

/* Called when a new track starts. */
void songchanged(int sig) {
	if(sig == SIGUSR1) {
		changed = !0;
		lastchange = time(NULL);
	}
}

void dying(int sig) {
	fprintf(stderr, "Caught signal %d. Trying to clean up. Please use Q in future!\n",
			sig);
	cleanup();
	exit(0);
}
