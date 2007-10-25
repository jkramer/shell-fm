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

#include "hash.h"
#include "service.h"
#include "interface.h"
#include "settings.h"
#include "autoban.h"
#include "sckif.h"
#include "playlist.h"
#include "submit.h"

#include "globals.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

unsigned flags = RTP;
time_t changeTime = 0, pausetime = 0;

static void cleanup(void);
static void forcequit(int);
static void help(const char *, int);

int main(int argc, char ** argv) {
	int option, nerror = 0, background = 0, haveSocket = 0;
	time_t pauselength = 0;
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
				help(argv[0], 0);
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
		help(argv[0], EXIT_FAILURE);


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
		int port = 54311;

		if(haskey(& rc, "port"))
			port = atoi(value(& rc, "port"));

		if(mksckif(value(& rc, "bind"), (unsigned short) port))
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
	loadqueue(!0);

	/* Set up signal handlers for communication with the playback process. */
	signal(SIGINT, forcequit);


	/* Authenticate to the Last.FM server. */
	if(!authenticate(value(& rc, "username"), value(& rc, "password")))
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
		enable(QUIET);
	}


	/* Play default radio, if specified. */
	if(haskey(& rc, "default-radio"))
		station(value(& rc, "default-radio"));


	/* The main loop. */
	while(!0) {
		pid_t child;
		int status, playnext = 0;


		/* Check if anything died (submissions fork or playback fork). */
		while((child = waitpid(-1, & status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
			if(child == subfork)
				subdead(WEXITSTATUS(status));
			else if(child == playfork) {
				if(WIFSTOPPED(status))
					time(& pausetime);
				else {
					if(WIFCONTINUED(status))
						pauselength += time(NULL) - pausetime;
					else
						playnext = !0;
					pausetime = 0;		
				}
			}
		}

		/*
			Check if the playback process died. If so, submit the data
			of the last played track. Then check if there are tracks left
			in the playlist; if not, try to refresh the list and stop the
			stream if there are no new tracks to fetch.
			Also handle user stopping the stream here.  We need to check for
			playnext != 0 before handling enabled(STOPPED) anyway, otherwise
			playfork would still be running.
		*/
		if(playnext) {
			unsigned
				duration = atoi(value(& track, "duration")) / 1000,
				played = time(NULL) - changeTime - pauselength;

			playfork = 0;

			if(enabled(RTP) && duration > 29 && (played >= 240 || played > (duration / 2)))
				enqueue(& track);

			submit(value(& rc, "username"), value(& rc, "password"));

			/* Check if the user stopped the stream. */
			if(enabled(STOPPED)) {
				freelist(& playlist);
				empty(& track);
				disable(STOPPED);
				continue;
			}
			
			shift(& playlist);
		}


		if(playnext || enabled(CHANGED)) {
			if(!playlist.left) {
				expand(& playlist);
				if(!playlist.left) {
					puts("No tracks left.");
					playnext = 0;
					disable(CHANGED);
					continue;
				}
			}

			if(!playfork) {
				if(play(& playlist)) {
					time(& changeTime);

					set(& track, "stationURL", currentStation);

					/* Print what's currently played. (Ondrej Novy) */
					if(!background) {
						if(enabled(CHANGED) && playlist.left > 0) {
							puts(meta("Receiving %s.", !0));
							disable(CHANGED);
						}

						if(haskey(& rc, "title-format"))
							printf("%s\n", meta(value(& rc, "title-format"), !0));
						else
							printf("%s\n", meta("Now playing \"%t\" by %a.", !0));
					}


					/* Write track data into a file. */
					if(haskey(& rc, "np-file") && haskey(& rc, "np-file-format")) {
						signed np;
						const char
							* file = value(& rc, "np-file"),
							* fmt = value(& rc, "np-file-format");

						unlink(file);
						if(-1 != (np = open(file, O_WRONLY | O_CREAT, 0644))) {
							const char * output = meta(fmt, 0);
							if(output)
								write(np, output, strlen(output));
							close(np);
						}
					}


					/* Run a command with our track data. */
					if(haskey(& rc, "np-cmd"))
						run(meta(value(& rc, "np-cmd"), 0));
				} else
					changeTime = 0;
			}

			if(banned(value(& track, "creator"))) {
				puts(meta("%a is banned.", !0));
				rate("B");
				fflush(stdout);
			}
		}

		playnext = 0;

		if(playfork && changeTime && haskey(& track, "duration") && !pausetime) {
			unsigned duration;
			signed remain;
			char remstr[32];

			duration = atoi(value(& track, "duration")) / 1000;

			remain = (changeTime + duration) - time(NULL) + pauselength;

			snprintf(remstr, sizeof(remstr), "%d", remain);
			set(& track, "remain", remstr);

			if(!background) {
				printf("%c%02d:%02d\r", remain < 0 ? '-' : ' ', remain / 60, remain % 60);
				fflush(stdout);
			}
		}

		interface(!background);
		if(haveSocket)
			sckif(background ? 2 : 0);
	}

	return 0;
}


static void help(const char * argv0, int errorCode) {
	fprintf(stderr,
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
	empty(& track);

	freelist(& playlist);

	if(currentStation) {
		free(currentStation);
		currentStation = NULL;
	}

	if(subfork)
		waitpid(subfork, NULL, 0);

	dumpqueue(!0);

	if(playfork)
		kill(playfork, SIGUSR1);
}


static void forcequit(int sig) {
	if(sig == SIGINT) {
		fputs("Try to press Q next time you want to quit.\n", stderr);
		exit(-1);
	}
}
