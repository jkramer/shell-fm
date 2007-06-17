/*
	vim:syntax=c tabstop=2 shiftwidth=2 noexpandtab
	
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

#include "hash.h"
#include "service.h"
#include "interface.h"
#include "settings.h"
#include "autoban.h"
#include "sckif.h"
#include "playlist.h"


#define PATH_MAX 4096

extern struct hash data, track;
extern struct playlist playlist;

extern pid_t playfork;
extern char * currentStation;
extern float avglag;

int stop = 0, discovery = 0, stationChanged = 0, record = !0, death = 0;
time_t changeTime = 0;

static void cleanup(void);
static void deadchild(int);
static void forcequit(int);
static void exitWithHelp(const char *, int);

int main(int argc, char ** argv) {
	int option, nerror = 0, background = 0, haveSocket = 0;
	char * proxy;
	opterr = 0;
	
	/* Load settings from ~/.shell-fm/shell-fm.rc. */
	settings(rcpath("shell-fm.rc"), !0);


	/* Get proxy environment variable. */
	if((proxy = getenv("http_proxy")) != NULL)
		set(& rc, "proxy", proxy);


	/* Parse through command line options. */
	while(-1 != (option = getopt(argc, argv, ":dhi:p:D:y:")))
		switch(option) {
			case 'd': /* Daemonize. */
				background = !0;
				break;

			case 'i': /* IP to bind network interface to. */
				set(& rc, "bind", optarg);
				break;

			case 'p': /* Port to listen on. */
				if(atoi(optarg))
					set(& rc, "port", optarg);
				else {
					fputs("Invalid port.\n", stderr);
					++nerror;
				}
				break;

			case 'D': /* Path to audio device file. */
				set(& rc, "device", optarg);
				break;

			case 'y': /* Proxy address. */
				set(& rc, "proxy", optarg);
				break;

			case 'h': /* Print help text and exit. */
				exitWithHelp(argv[0], 0);
				break;

			case ':':
				fprintf(stderr, "Missing argument for option -%c.\n\n", optopt);
				++nerror;
				break;

			case '?':
			default:
				fprintf(stderr, "Unknown option -%c.\n\n", optopt);
				++nerror;
				break;
		}

	/* The next argument, if present, is the lastfm:// URL we want to play. */
	if(optind > 0 && optind < argc && argv[optind]) {
		const char * station = argv[optind];

		if(0 != strncmp(station, "lastfm://", 9)) {
			fprintf(stderr, "Not a valid lastfm url: %s\n\n", station);
			++nerror;
		} else {
			set(& rc, "default-radio", station);
		}
	}

	
	if(nerror)
		exitWithHelp(argv[0], EXIT_FAILURE);


	/* The -D / device option is only used if have libao support. */
#ifndef __HAVE_LIBAO__ 
	if(!haskey(& rc, "device"))
		set(& rc, "device", "/dev/audio");
#endif


	puts("Shell.FM v" PACKAGE_VERSION ", (C) 2007 by Jonas Kramer");
	puts("Published under the terms of the GNU General Public License (GPL)\n");


	if(!background)
		puts("Press ? for help.\n");
	

	/* Open a port so Shell.FM can be controlled over network. */
	if(haskey(& rc, "bind")) {
		unsigned short port =
			haskey(& rc, "port") ? atoi(value(& rc, "port")) : 54311;
		if(mksckif(value(& rc, "bind"), port))
			haveSocket = !0;
	}


	/* We can't daemonize if there's no possibility left to control Shell.FM. */
	if(background && !haveSocket) {
		fputs("Can't daemonize without control socket.", stderr);
		exit(EXIT_FAILURE);
	}


	/* Ask for username/password if they weren't specified in the .rc file. */
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
	memset(& playlist, 0, sizeof(struct playlist));
	
	atexit(cleanup);


	/* Set up signal handlers for communication with the playback process. */
	signal(SIGCHLD, deadchild);
	signal(SIGINT, forcequit);


	/* Authenticate to the Last.FM server. */
	if(!handshake(value(& rc, "username"), value(& rc, "password")))
		exit(EXIT_FAILURE);

	/* Store session key for use by external tools. */
	if(haskey(& data, "session")) {
		FILE * fd = fopen(rcpath("session"), "w");
		if(fd) {
			fprintf(fd, "%s\n", value(& data, "session"));
			fclose(fd);
		}
	}


	/* Fork to background. */
	if(background) {
		pid_t pid = fork();
		if(pid == -1) {
			fputs("Failed to daemonize.\n", stderr);
			exit(EXIT_FAILURE);
		} else if(pid) {
			exit(EXIT_SUCCESS);
		}
	}


	/* Play default radio, if specified. */
	if(haskey(& rc, "default-radio"))
		station(value(& rc, "default-radio"));


	/* The main loop. */
	while(!0) {
		/* Check if our child process died. */
		if(death || stationChanged) {
			pid_t pid;
			int status;

			death = 0;

			time(& changeTime);

			while((pid = waitpid(-1, & status, WNOHANG)) > 0)
				(pid == playfork && playfork) && (playfork = 0);
		

			/*
				The station function already ran the play function, so don't call it
				again here. If it's not a new station, this means that the previously
				played track ended, so remove it from the playlist. If the stream
				was not stopped by the user, we play the next track.
			*/
			if(!stationChanged) {
				shift(& playlist);
				if(stop) {
					freelist(& playlist);
					empty(& track);
					stop = 0;
					continue;
				} else if(!play(& playlist))
					continue;
			}


			/*
				Check if the artist of the current track is marked as auto-banned.
				If so, send the ban control to Last.FM and skip the track.
			*/
			if(banned(meta("%a", 0))) {
				if(control("ban"))
					puts(meta("\"%t\" by %a auto-banned.", !0));
				else
					puts(meta("Failed to auto-ban \"%t\" by %a.", !0));

				if(playfork)
					kill(playfork, SIGKILL);
			}

			
			if(stationChanged) {
				if(!background && playlist.left > 0)
					puts(meta("Receiving %s.", !0));

				stationChanged = 0;
			}


			/* Print what's currently played. (Ondrej Novy) */
			if(!background && playlist.left > 0) {
				if(haskey(& rc, "title-format"))
					printf("%s\n", meta(value(& rc, "title-format"), !0));
				else
					printf("%s\n", meta("Now playing \"%t\" by %a.", !0));
			}


			/* Write NP line to a file if wanted. */
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

			/* Run a command with our track data. */
			if(haskey(& rc, "np-cmd"))
				run(meta(value(& rc, "np-cmd"), 0));
		}

		if(playfork && changeTime && haskey(& track, "duration")) {
			int rem;
			char remstr[32] = { 0 };

			rem = 
				(changeTime + atoi(value(& track, "duration")) / 1000) - time(NULL);

			snprintf(remstr, 32, "%d", rem);

			set(& track, "remain", remstr);

			if(!background) {
				printf("[%.2f] %c%02d:%02d\r", avglag, rem < 0 ? '-' : ' ', rem / 60, rem % 60);
				fflush(stdout);
			}
		}
		
		interface(!background);
		if(haveSocket)
			sckif(background ? 2 : 0);
	}
	
	return 0;
}


static void exitWithHelp(const char * argv0, int errorCode) {
	FILE * out = errorCode ? stderr : stdout;

	fprintf(out,
		"shell-fm - Copyright (C) 2007 by Jonas Kramer\n"
		"\n"
		"%s [options] [lastfm://url]\n"
		"\n"
		"  -d        daemon mode.\n"
		"  -i        address to listen on.\n"
		"  -p        port to listen on.\n"
		"  -D        device to play on.\n"
		"  -y        proxy url to connect through.\n"
		"  -h        this help.\n",
		argv0
	);

	exit(errorCode);
}


static void cleanup(void) {
	canon(!0);
	rmsckif();

	empty(& data);
	empty(& rc);

	if(currentStation)
		free(currentStation);
	
	if(playfork)
		kill(playfork, SIGTERM);
}


static void deadchild(int sig) {
	sig == SIGCHLD && (death = !0);
}


static void forcequit(int sig) {
	if(sig == SIGINT) {
		fputs("Try to press Q next time you want to quit.\n", stderr);
		exit(-1);
	}
}


