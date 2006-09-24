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
#include "include/sckif.h"

#define PATH_MAX 4096

extern struct hash data, track;
extern pid_t playfork;

int changed = 0, discovery = 0, stationChanged = 0, record = !0, death = 0;
unsigned paused = 0;
time_t changeTime = 0, pausetime = 0;

void cleanup(void);
void deadchild(int);
void songchanged(int);
void forcequit(int);

int main(int argc, char ** argv) {
	int option, nerror = 0, daemon = 0, haveSocket = 0;
	opterr = 0;
	
	settings(rcpath("shell-fm.rc"), !0);
	
	while(-1 != (option = getopt(argc, argv, ":di:p:D:")))
		switch(option) {
			case 'd':
				daemon = !0;
				break;
			case 'i':
				set(& rc, "bind", optarg);
				break;
			case 'p':
				if(atoi(optarg))
					set(& rc, "port", optarg);
				else {
					fputs("Invalid port.\n", stderr);
					++nerror;
				}
				break;
			case ':':
				fprintf(stderr, "Missing argument for option -%c.\n", optopt);
				++nerror;
				break;
			case 'D':
				set(& rc, "device", optarg);
				break;
			case '?':
			default:
				fprintf(stderr, "Unknown option -%c.\n", optopt);
				++nerror;
				break;
		}

	if(nerror)
		exit(EXIT_FAILURE);
	
	if(!haskey(& rc, "device"))
		set(& rc, "device", "/dev/audio");

	puts("Shell.FM v" VERSION ", written 2006 by Jonas Kramer");
	puts("Published under the terms of the GNU General Public License (GPL)\n");

	if(!daemon)
		puts("Press ? for help.\n");
	
	if(haskey(& rc, "bind")) {
		unsigned short port =
			haskey(& rc, "port") ? atoi(value(& rc, "port")) : 54311;
		if(mksckif(value(& rc, "bind"), port))
			haveSocket = !0;
	}

	if(daemon && !haveSocket) {
		fputs("Can't daemonize without control socket.", stderr);
		exit(EXIT_FAILURE);
	}

	if(!haskey(& rc, "password")) {
		char * password;
		
		if(!haskey(& rc, "username")) {
			char username[256] = { 0 };
			fputs("Login: ", stderr);
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
	signal(SIGINT, forcequit);

	if(!handshake(value(& rc, "username"), value(& rc, "password")))
		exit(EXIT_FAILURE);

	if(daemon) {
		pid_t pid = fork();
		if(pid == -1) {
			fputs("Failed to daemonize.\n", stderr);
			exit(EXIT_FAILURE);
		} else if(pid) {
			exit(EXIT_SUCCESS);
		}
	} else {
		using_history();
		read_history(rcpath("radio-history"));
	}

	if(haskey(& rc, "default-radio"))
		station(value(& rc, "default-radio"));

	while(!0) {
		if(death) {
			pid_t pid;
			int status;

			death = 0;
			while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				(pid == playfork && playfork) && (playfork = 0);
				if(WIFSIGNALED(status) && !playfork) {
					paused = !paused;
					if(!daemon) {
						fputs("Stopped.\r", stdout);
						fflush(stdout);
					}
				}
			}
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
				daemon || puts(meta("Receiving %s.", !0));
				stationChanged = 0;
			}

			if(!daemon) {
				if(haskey(&rc, "title-format"))
					printf("%s\n", meta(value(& rc, "title-format"), !0));
				else
					printf("%s\n", meta("Now playing \"%t\" by %a.", !0));
			}

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

		if(playfork && changeTime && haskey(& track, "trackduration") && !paused) {
			int rem;
			if(pausetime) {
				changeTime += time(NULL) - pausetime;
				pausetime = 0;
			}


			if(!daemon) {
				rem = 
					(changeTime + atoi(value(& track, "trackduration"))) - time(NULL);
				printf("%c%02d:%02d\r", rem < 0 ? '-' : ' ', rem / 60, rem % 60);
				fflush(stdout);
			}
		} else {
			if(paused && playfork) {
				if(!pausetime)
					pausetime = time(NULL);
				if(!daemon) {
					fputs("Paused.\r", stdout);
					fflush(stdout);
				}
			}
		}
		
		interface(!daemon);
		if(haveSocket)
			sckif();
	}
	
	return 0;
}


void cleanup(void) {
	fputs("Exiting...\n", stdout);

	canon(!0);
	rmsckif();

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

void forcequit(int sig) {
	if(sig == SIGINT) {
		fputs("Try to press Q next time you want to quit.\n", stderr);
		exit(-1);
	}
}
