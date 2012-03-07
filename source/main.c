/*
	Copyright (C) 2006-2010 by Jonas Kramer
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
#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>

#include "hash.h"
#include "service.h"
#include "interface.h"
#include "settings.h"
#include "autoban.h"
#include "sckif.h"
#include "playlist.h"
#include "submit.h"
#include "readline.h"
#include "radio.h"
#include "select.h"
#include "history.h"
#include "strary.h"

#include "globals.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef __NetBSD__
#  ifndef WCONTINUED
#    define WCONTINUED 0                    /* not available on NetBSD */
#  endif
#  ifndef WIFCONTINUED
#    define WIFCONTINUED(x)  ((x) == 0x13)  /* SIGCONT */
#  endif
#endif

#if !defined(WCONTINUED) || !defined(WIFCONTINUED)
#   undef  WCONTINUED
#   define WCONTINUED   0
#   undef  WIFCONTINUED
#   define WIFCONTINUED(wstat)  (0)
#endif

unsigned flags = RTP;
time_t change_time = 0, pausetime = 0;
char * nextstation = NULL;

int batch = 0, error = 0, delayquit = 0;

static void cleanup(void);
static void cleanup_term(void);
static void forcequit(int);
static void help(const char *, int);
static void playsig(int);
static void stopsig(int);

pid_t ppid = 0;

int main(int argc, char ** argv) {
	int option, nerror = 0, background = 0, quiet = 0, have_socket = 0;
	time_t pauselength = 0;
	char * proxy;
	opterr = 0;

	/* Create directories. */
	makercd();

	/* Load settings from ~/.shell-fm/shell-fm.rc. */
	settings(rcpath("shell-fm.rc"), !0);

	/* Enable discovery by default if it is set in configuration. */
	if(haskey(& rc, "discovery"))
		enable(DISCOVERY);

	/* Disable RTP if option "no-rtp" is set to something. */
	if(haskey(& rc, "no-rtp"))
		disable(RTP);

	/* If "daemon" is set in the configuration, enable daemon mode by default. */
	if(haskey(& rc, "daemon"))
		background = !0;

	/* If "quiet" is set in the configuration, enable quiet mode by default. */
	if(haskey(& rc, "quiet"))
		quiet = !0;

	/* Get proxy environment variable. */
	if((proxy = getenv("http_proxy")) != NULL)
		set(& rc, "proxy", proxy);


	/* Parse through command line options. */
	while(-1 != (option = getopt(argc, argv, ":dbhqi:p:D:y:")))
		switch(option) {
			case 'd': /* Daemonize. */
				background = !background;
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

			case 'b': /* Batch mode */
				batch = !0;
				break;

			case 'D': /* Path to audio device file. */
				set(& rc, "device", optarg);
				break;

			case 'y': /* Proxy address. */
				set(& rc, "proxy", optarg);
				break;

			case 'q': /* Quiet mode. */
				quiet = !quiet;
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
				fprintf(stderr, "Unknown option -%c.\n", optopt);
				++nerror;
				break;
		}

	/* The next argument, if present, is the lastfm:// URL we want to play. */
	if(optind > 0 && optind < argc && argv[optind]) {
		const char * station = argv[optind];

		set(& rc, "default-radio", station);
	}


	if(nerror)
		help(argv[0], EXIT_FAILURE);

#ifdef EXTERN_ONLY
	/* Abort if EXTERN_ONLY is defined and no extern command is present */
	if(!haskey(& rc, "extern")) {
		fputs("Can't continue without extern command.\n", stderr);
		exit(EXIT_FAILURE);
	}
#else
#ifndef LIBAO
	if(!haskey(& rc, "device"))
		set(& rc, "device", "/dev/audio");
#endif
#endif

	if(!background && !quiet) {
		puts("Shell.FM v" PACKAGE_VERSION ", (C) 2006-2010 by Jonas Kramer");
		puts("Published under the terms of the GNU General Public License (GPL).");

#ifndef TUXBOX
		puts("\nPress ? for help.\n");
#else
		puts("Compiled for the use with Shell.FM Wrapper.\n");
#endif
		fflush(stdout);
	}


	/* Open a port so Shell.FM can be controlled over network. */
	if(haskey(& rc, "bind")) {
		int port = 54311;

		if(haskey(& rc, "port"))
			port = atoi(value(& rc, "port"));

		if(tcpsock(value(& rc, "bind"), (unsigned short) port))
			have_socket = !0;
	}


	/* Open a UNIX socket for local "remote" control. */
	if(haskey(& rc, "unix") && unixsock(value(& rc, "unix")))
		have_socket = !0;


	/* We can't daemonize if there's no possibility left to control Shell.FM. */
	if(background && !have_socket) {
		fputs("Can't daemonize without control socket.\n", stderr);
		exit(EXIT_FAILURE);
	}


	/* Ask for username/password if they weren't specified in the .rc file. */
	if(!haskey(& rc, "password") && !haskey(& rc, "password-md5")) {
		char * password;

		if(!haskey(& rc, "username")) {
			char username[256] = { 0 };

			struct prompt prompt = {
				.prompt = "Login: ",
				.line = getenv("USER"), .history = NULL, .callback = NULL,
			};

			strncpy(username, readline(& prompt), 255);

			set(& rc, "username", username);
		}

		if(!(password = getpass("Password: ")))
			exit(EXIT_FAILURE);

		set(& rc, "password", password);
	}


	memset(& data, 0, sizeof(struct hash));
	memset(& track, 0, sizeof(struct hash));
	memset(& playlist, 0, sizeof(struct playlist));

	/* Fork to background. */
	if(background) {
		int null;
		pid_t pid = fork();

		if(pid == -1) {
			fputs("Failed to daemonize.\n", stderr);
			exit(EXIT_FAILURE);
		} else if(pid) {
			exit(EXIT_SUCCESS);
		}

		enable(QUIET);

		/* Detach from TTY */
		setsid();
		pid = fork();

		if(pid > 0)
			exit(EXIT_SUCCESS);

		/* Close stdin out and err */
		close(0);
		close(1);
		close(2);

		/* Redirect  stdin and out to /dev/null */
		null = open("/dev/null", O_RDWR);
		dup(null);
		dup(null);
	}

	ppid = getpid();

	atexit(cleanup);
	loadqueue(!0);

	/* Set up signal handlers for communication with the playback process. */
	signal(SIGINT, forcequit);

	/* SIGUSR2 from playfork means it detected an error. */
	signal(SIGUSR2, playsig);

	/* Catch SIGTSTP to set pausetime when user suspends us with ^Z. */
	signal(SIGTSTP, stopsig);


	/* Authenticate to the Last.FM server. */
	if(haskey(& rc, "password-md5") && !authenticate(value(& rc, "username"), value(& rc, "password-md5")))
		exit(EXIT_FAILURE);
	else if (!haskey(& rc, "password-md5") && !authenticate_plaintext(value(& rc, "username"), value(& rc, "password")))
		exit(EXIT_FAILURE);

	/* Store session key for use by external tools. */
	if(haskey(& data, "session")) {
		FILE * fd = fopen(rcpath("session"), "w");
		if(fd) {
			fprintf(fd, "%s\n", value(& data, "session"));
			fclose(fd);
		}
	}

	if(!background) {
		struct input keyboard = { 0, KEYBOARD };
		register_handle(keyboard);
		canon(0);
		atexit(cleanup_term);
	}


	/* Play default radio, if specified. */
	if(haskey(& rc, "default-radio")) {
		if(!strcmp(value(& rc, "default-radio"), "last")) {
			char ** history = load_history(), * last = NULL, ** p;

			for(p = history; * p != NULL; ++p) {
				last = * p;
			}

			set(& rc, "default-radio", last);
			purge(history);
		}

		station(value(& rc, "default-radio"));
	}

	else if(!background)
		radioprompt("radio url> ");

	/* The main loop. */
	while(!0) {
		pid_t child;
		int status, playnext = 0;

		/* Check if anything died (submissions fork or playback fork). */
		while((child = waitpid(-1, & status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
			if(child == subfork)
				subdead(WEXITSTATUS(status));
			else if(child == playfork) {
				if(WIFSTOPPED(status)) {
					/* time(& pausetime); */
				}
				else {
					if(WIFCONTINUED(status)) {
						signal(SIGTSTP, stopsig);
						if(pausetime != 0) {
							pauselength += time(NULL) - pausetime;
						}
					}
					else {
						playnext = !0;
						unlinknp();

						if(delayquit) {
							quit();
						}
					}
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
			playfork = 0;

			if(enabled(RTP)) {
				unsigned duration, played, minimum;

				duration = atoi(value(& track, "duration"));
				played = time(NULL) - change_time - pauselength;

				/* Allow user to specify minimum playback length (min. 50%). */
				if(haskey(& rc, "minimum")) {
					unsigned percent = atoi(value(& rc, "minimum"));
					if(percent < 50)
						percent = 50;
					minimum = duration * percent / 100;
				}
				else {
					minimum = duration / 2;
				}

				if(duration >= 30 && (played >= 240 || played > minimum))
					enqueue(& track);
			}

			submit(value(& rc, "username"), value(& rc, "password"));

			/* Check if the user stopped the stream. */
			if(enabled(STOPPED) || error) {
				freelist(& playlist);
				empty(& track);

				if(error) {
					fputs("Playback stopped with an error.\n", stderr);
					error = 0;
				}

				disable(STOPPED);
				disable(CHANGED);

				continue;
			}

			shift(& playlist);
		}


		if(playnext || enabled(CHANGED)) {
			if(nextstation != NULL) {
				playnext = 0;
				disable(CHANGED);

				if(!station(nextstation))
					enable(STOPPED);

				free(nextstation);
				nextstation = NULL;
			}

			if(!enabled(STOPPED) && !playlist.left) {
				expand(& playlist);
				if(!playlist.left) {
					puts("No tracks left.");
					playnext = 0;
					disable(CHANGED);
					continue;
				}
			}

			if(!playfork) {
				/*
					If there was a track played before, and there is a gap
					configured, wait that many seconds before playing the next
					track.
				*/
				if(playnext && !enabled(INTERRUPTED) && haskey(& rc, "gap")) {
					int gap = atoi(value(& rc, "gap"));

					if(gap > 0)
						sleep(gap);
				}

				disable(INTERRUPTED);

				if(play(& playlist)) {
					time(& change_time);
					pauselength = 0;

					set(& track, "stationURL", current_station);

					/* Print what's currently played. (Ondrej Novy) */
					if(!background) {
						if(enabled(CHANGED) && playlist.left > 0) {
							puts(meta("Receiving %s.", M_COLORED, & track));
							disable(CHANGED);
						}

						if(haskey(& rc, "title-format"))
							printf("%s\n", meta(value(& rc, "title-format"), M_COLORED, & track));
						else
							printf("%s\n", meta("Now playing \"%t\" by %a.", M_COLORED, & track));
					}

					if(enabled(RTP)) {
						notify_now_playing(
							& track,
							value(& rc, "username"),
							value(& rc, "password")
						);
					}


					/* Write track data into a file. */
					if(haskey(& rc, "np-file") && haskey(& rc, "np-file-format")) {
						signed np;
						const char
							* file = value(& rc, "np-file"),
							* fmt = value(& rc, "np-file-format");

						unlink(file);
						if(-1 != (np = open(file, O_WRONLY | O_CREAT, 0644))) {
							const char * output = meta(fmt, 0, & track);
							if(output)
								write(np, output, strlen(output));
							close(np);
						}
					}


					if(haskey(& rc, "screen-format")) {
						const char * term = getenv("TERM");
						if(term != NULL && !strncmp(term, "screen", 6)) {
							const char * output =
								meta(value(& rc, "screen-format"), 0, & track);
							printf("\x1Bk%s\x1B\\", output);
						}
					}


					if(haskey(& rc, "term-format")) {
						const char * output =
							meta(value(& rc, "term-format"), 0, & track);
						printf("\x1B]2;%s\a", output);
					}


					/* Run a command with our track data. */
					if(haskey(& rc, "np-unescaped-cmd"))
						run(meta(value(& rc, "np-unescaped-cmd"), 0, & track));
					if(haskey(& rc, "np-cmd"))
						run(meta(value(& rc, "np-cmd"), M_SHELLESC, & track));
				} else
					change_time = 0;
			}

			if(banned(value(& track, "creator"))) {
				puts(meta("%a is banned.", M_COLORED, & track));
				rate("B");
				fflush(stdout);
			}
		}

		playnext = 0;

		if(playfork && change_time && haskey(& track, "duration") && !pausetime) {
			unsigned duration;
			signed remain;
			char remstr[32];

			duration = atoi(value(& track, "duration"));

			remain = (change_time + duration) - time(NULL) + pauselength;

			snprintf(remstr, sizeof(remstr), "%d", remain);
			set(& track, "remain", remstr);

			if(!background) {
				printf(
					"%s%c",
					strdup(meta("%r", M_COLORED, & track)),
					// strdup(meta("%v", M_COLORED, & track)),
					batch ? '\n' : '\r'
				);
				fflush(stdout);
			}
		}

		handle_input(1000000);
	}

	return 0;
}


static void help(const char * argv0, int error_code) {
	fprintf(stderr,
		"Shell.FM v" PACKAGE_VERSION ", (C) 2006-2010 by Jonas Kramer\n"
		"Published under the terms of the GNU General Public License (GPL).\n"
		"\n"
		"%s [options] [lastfm://url]\n"
		"\n"
		"  -d        daemon mode.\n"
		"  -q        quiet mode.\n"
		"  -i        address to listen on.\n"
		"  -p        port to listen on.\n"
		"  -b        batch mode.\n"
		"  -D        device to play on.\n"
		"  -y        proxy url to connect through.\n"
		"  -h        this help.\n",
		argv0
	);

	exit(error_code);
}


static void cleanup(void) {
	rmsckif();

	if(haskey(& rc, "unix") && getpid() == ppid)
		unlink(value(& rc, "unix"));

	empty(& data);
	empty(& rc);
	empty(& track);

	freelist(& playlist);

	if(current_station) {
		free(current_station);
		current_station = NULL;
	}

	if(subfork)
		waitpid(subfork, NULL, 0);

	dumpqueue(!0);

	/* Clean cache. */
	if(!access(rcpath("cache"), R_OK | W_OK | X_OK)) {
		const char * cache = rcpath("cache");
		DIR * directory = opendir(cache);

		if(directory != NULL) {
			time_t expiry = 24 * 60 * 60; /* Expiration after 24h. */
			struct dirent * entry;
			struct stat status;
			char path[PATH_MAX];

			if(haskey(& rc, "expiry"))
				expiry = atoi(value(& rc, "expiry"));

			while((entry = readdir(directory)) != NULL) {
				snprintf(path, sizeof(path), "%s/%s", cache, entry->d_name);

				if(!stat(path, & status)) {
					if(status.st_mtime < (time(NULL) - expiry)) {
						unlink(path);
					}
				}
			}

			closedir(directory);
		}
	}

	if(playfork)
		kill(playfork, SIGUSR1);
}

static void cleanup_term(void) {
	if (getpid() == ppid) {
		canon(1);
	}
}


static void forcequit(int sig) {
	if(sig == SIGINT) {
		fputs("Try to press Q next time you want to quit.\n", stderr);
		exit(EXIT_FAILURE);
	}
}


static void playsig(int sig) {
	if(sig == SIGUSR2) {
		error = !0;
		enable(INTERRUPTED);
	}
}


static void stopsig(int sig) {
	if(sig == SIGTSTP) {
		time(& pausetime);

		signal(SIGTSTP, SIG_DFL);
		raise(SIGTSTP);
	}
}

