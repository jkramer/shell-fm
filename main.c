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

int changed = 0, discovery = 0, stationChanged = 0, record = !0, death = 0;
time_t changeTime = 0;

void cleanup(void);
void deadchild(int);
void songchanged(int);
void pebcak(int);


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

			fputs("Login: ", stdout);
			if(!scanf("%255s", username))
				exit(EXIT_FAILURE);

			set(& rc, "username", username);
		}
		
		if(!(password = getpass("Password: ")))
			exit(EXIT_FAILURE);

		set(& rc, "password", password);
	}

	memset(& data, 0, sizeof(struct hash));
	memset(& track, 0, sizeof(struct hash));
	
	atexit(cleanup);
	
	signal(SIGCHLD, deadchild);
	signal(SIGUSR1, songchanged);
	signal(SIGINT, pebcak);

	if(!handshake(value(& rc, "username"), value(& rc, "password")))
		exit(EXIT_FAILURE);

	if(argc == 2)
		station(argv[1]);
	else if(haskey(& rc, "default-radio"))
		station(value(& rc, "default-radio"));

	using_history();
	read_history(rcpath("radio-history"));


	while(!0) {
		if(death) {
			pid_t pid;
			death = 0;
			while((pid = waitpid(-1, NULL, WNOHANG)) > 0)
				(pid == playfork && playfork) && (playfork = 0);
		}
		
		if(changed) {
			char * last = strdup(meta("%a %t", 0));
			unsigned retries = 0;

			if(last) {
				while(retries < 10 && !strcmp(last, meta("%a %t", 0)))
					update(& track) || ++retries;
				free(last);
			}

			if(banned(meta("%a", 0))) {
				const char * msg = meta(control("ban")
						? "\"%t\" by %a auto-banned."
						: "Failed to auto-ban \"%t\" by %a.", !0);
				puts(msg);
				changed = 0;
				continue;
			}
			
			if(stationChanged) {
				puts(meta("Receiving %s.", !0));
				stationChanged = 0;
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
					output && write(np, output, strlen(output));
					close(np);
				}
			}

			if(haskey(& rc, "np-cmd"))
				run(meta(value(& rc, "np-cmd"), 0));
		}

		if(playfork && changeTime && haskey(& track, "trackduration")) {
			int rem =
				(changeTime + atoi(value(& track, "trackduration"))) - time(NULL);
			printf("%c%02d:%02d\r", rem < 0 ? '-' : ' ', rem / 60, rem % 60);
			fflush(stdout);
		}

		interface(!0);
	}
	
	return 0;
}


void cleanup(void) {
	canon(!0);
	write_history(rcpath("radio-history"));

	empty(& data);
	empty(& rc);
	
	playfork && kill(playfork, SIGTERM);
}


void deadchild(int sig) {
	sig == SIGCHLD && (death = !0);
}


void songchanged(int sig) {
	if(sig == SIGUSR1) {
		changed = !0;
		changeTime = time(NULL);
	}
}


void pebcak(int sig) {
	sig == SIGINT && fputs("Please use Q to quit.\n", stderr);
}
