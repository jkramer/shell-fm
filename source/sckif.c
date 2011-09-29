/*
	Copyright (C) 2006-2010 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>
#include <sys/un.h>

#include "sckif.h"
#include "http.h"
#include "service.h"
#include "interface.h"
#include "hash.h"
#include "submit.h"
#include "getln.h"
#include "tag.h"
#include "select.h"
#include "util.h"
#include "ropen.h"

#include "globals.h"

#include "split.h"
#include "ropen.h"

struct hash track;

static int stcpsck = -1, sunixsck = -1;

#define BUFSIZE 1024

void parse_volume(const char *);


int tcpsock(const char * ip, unsigned short port) {
	static const int one = 1;
	struct sockaddr_in host;
	struct hostent * hostent;

	if(!ip || !port)
		return 0;

	if(-1 == (stcpsck = socket(AF_INET, SOCK_STREAM, PF_UNSPEC))) {
		fputs("Failed to create socket.\n", stderr);
		return 0;
	}

	if(!(hostent = gethostbyname(ip))) {
		fprintf(stderr, "Failed to lookup host. %s.\n", hstrerror(h_errno));
		return 0;
	}

	host.sin_family = PF_INET;
	host.sin_port = htons(port);
	host.sin_addr.s_addr = * (unsigned *) hostent->h_addr;

	if(-1 == setsockopt(stcpsck, SOL_SOCKET, SO_REUSEADDR, & one, sizeof one))
		fprintf(stderr, "Couldn't make socket re-usable. %s\n", strerror(errno));

	if(bind(stcpsck, (struct sockaddr *) & host, sizeof(struct sockaddr_in))) {
		fprintf(stderr, "Failed to bind socket. %s.\n", strerror(errno));
		return 0;
	}

	listen(stcpsck, 2);

	register_listen_socket(stcpsck);

	return !0;
}


int unixsock(const char * path) {
	struct sockaddr_un host;

	if(path == NULL)
		return 0;


	if(!access(path, F_OK)) {
		fprintf(stderr, "%s already existing. UNIX socket not created.\n", path);
		return 0;
	}


	if(-1 == (sunixsck = socket(AF_UNIX, SOCK_STREAM, PF_UNSPEC))) {
		fputs("Failed to create socket.\n", stderr);
		return 0;
	}


	memset(& host, 0, sizeof(struct sockaddr_un));
	strncpy(host.sun_path, path, sizeof(host.sun_path) - 1);
	host.sun_family = AF_UNIX;


	if(bind(sunixsck, (struct sockaddr *) & host, sizeof(struct sockaddr_un))) {
		fprintf(stderr, "Failed to bind socket. %s.\n", strerror(errno));
		return 0;
	}

	listen(sunixsck, 2);

	register_listen_socket(sunixsck);

	return !0;
}


void rmsckif(void) {
	if(stcpsck > 0) {
		close(stcpsck);
		stcpsck = -1;
	}

	if(sunixsck > 0) {
		close(sunixsck);
		sunixsck = -1;
	}
}


void accept_client(int listen_socket) {
	struct sockaddr client;
	socklen_t client_size = sizeof(struct sockaddr);

	int client_socket = accept(listen_socket, & client, & client_size);

	if(-1 != client_socket)
		register_client_socket(client_socket);
}


void handle_client(int client_socket) {
	static FILE * fd = NULL;
	int disconnect = 0;

	if (!fd)
		fd = fdopen(client_socket, "rw");

	signal(SIGPIPE, SIG_IGN);

	if(fd != NULL && !feof(fd)) {
		debug("client socket is ok and readable: %i\n", client_socket);
		char line[BUFSIZE];

		if(fgets(line, sizeof(line), fd) == NULL) {
			disconnect = 1;
		}
		else {
			if(!ferror(fd)) {
				unsigned chunks, i;
				char ** lines = split(line, "\n", & chunks);

				for(i = 0; i < chunks && !disconnect; ++i) {
					char reply[BUFSIZE] = { 0, };
					debug("client message: <%s>\n", lines[i]);

					disconnect = execcmd(lines[i], reply);

					if(strlen(reply)) {
						strncat(reply, "\n", BUFSIZE - strlen(reply));
						write(client_socket, reply, strlen(reply));
					}
				}
			}

			else {
				debug("fd error: %i\n", ferror(fd));
				disconnect = 1;
			}

		}
	}

	if(disconnect) {
		debug("removing client\n");
		fshutdown(& fd);
		remove_handle(client_socket);
	}
}


int execcmd(const char * cmd, char * reply) {
	char arg[1024], * ptr;
	unsigned ncmd;
	const char * known [] = {
		"play",
		"love",
		"ban",
		"skip",
		"quit",
		"info",
		"pause",
		"discovery",
		"tag-artist",
		"tag-album",
		"tag-track",
		"artist-tags",
		"album-tags",
		"track-tags",
		"stop",
		"volume-up",
		"volume-down",
		"volume",
		"rtp",
		"status",
		"detach"
	};

	memset(arg, 0, sizeof(arg));
	memset(reply, 0, BUFSIZE);

	for(ncmd = 0; ncmd < (sizeof(known) / sizeof(char *)); ++ncmd) {
		if(!strncmp(known[ncmd], cmd, strlen(known[ncmd])))
			break;
	}

	switch(ncmd) {
		case (sizeof(known) / sizeof(char *)):
			strncpy(reply, "ERROR", BUFSIZE);
			break;

		/* "play lastfm://station" */
		case 0:
			if(sscanf(cmd, "play %128[a-zA-Z0-9:/_ %,*.-]", arg) == 1) {
				char * url;
				decode(arg, & url);
				station(url);
				free(url);
			}
			break;

		/* Love currently played track. */
		case 1:
			rate("L");
			break;

		/* Ban currently played track. */
		case 2:
			rate("B");
			break;

		/* Skip track. */
		case 3:
			rate("S");
			break;

		/* Kill Shell.FM. */
		case 4:
			quit();

		/* "info FORMAT" - returns the format string with the meta data filled in. */
		case 5:
			if(* (cmd + 5))
				strncpy(reply, meta(cmd + 5, 0, & track), BUFSIZE);
			else if(haskey(& rc, "np-file-format"))
				strncpy(
					reply,
					meta(value(& rc, "np-file-format"), 0, & track),
					BUFSIZE
				);

			break;

		/* Pause playback. */
		case 6:
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

		/* Toggle discovery mode. Returns "DISCOVERY <ON|OFF>" */
		case 7:
			toggle(DISCOVERY);
			snprintf(
				reply,
				BUFSIZE,
				"DISCOVERY %s",
				enabled(DISCOVERY) ? "ON" : "OFF"
			);
			break;

		/* "tag-artist tag1,tag2,..." - tag the artist of the current track. */
		case 8:
			if(sscanf(cmd, "tag-artist %128s", arg) == 1)
				sendtag('a', arg, track);
			break;

		/* "tag-album tag1,tag2,..." - tag the album of the current track. */
		case 9:
			if(sscanf(cmd, "tag-album %128s", arg) == 1)
				sendtag('l', arg, track);
			break;

		/* "tag-track tag1,tag2,..." - tag the current track. */
		case 10:
			if(sscanf(cmd, "tag-track %128s", arg) == 1)
				sendtag('t', arg, track);
			break;

		/* Return comma-separated list of the current artists tags. */
		case 11:
			if((ptr = oldtags('a', track)) != NULL) {
				strncpy(reply, ptr, BUFSIZE);
				free(ptr);
				ptr = NULL;
			}
			break;

		/* Return comma-separated list of the current albums tags. */
		case 12:
			if((ptr = oldtags('l', track)) != NULL) {
				strncpy(reply, ptr, BUFSIZE);
				free(ptr);
				ptr = NULL;
			}
			break;

		/* Return comma-separated list of the current tracks tags. */
		case 13:
			if((ptr = oldtags('t', track)) != NULL) {
				strncpy(reply, ptr, BUFSIZE);
				free(ptr);
				ptr = NULL;
			}
			break;


		/* Stop playback. */
		case 14:
			if(playfork) {
				enable(STOPPED);
				kill(playfork, SIGUSR1);
			}
			break;

		/* Increase absolute volume (0-64) by 1. */
		case 15:
			volume_up();
			break;

		/* Decrease absolute volume (0-64) by 1. */
		case 16:
			volume_down();
			break;

		/*
			Set volume.
			"volume 32" - set absolute volume (0-64) to 32 (50%).
			"volume %50" - same, but using percentual volume.
			"volume +1" - same as "volume_up".
			"volume -1" - same as "volume_down".

			Returns absolute volume ("VOLUME 32").
		*/
		case 17:
			parse_volume(cmd);
			snprintf(reply, BUFSIZE, "VOLUME %d", volume);
			break;

		/* Toggle RTP (report to profile, "scrobbling"). Returns "RTP <ON|OFF>". */
		case 18:
			/* RTP on/off */
			toggle(RTP);
			snprintf(reply, BUFSIZE, "RTP %s", enabled(RTP) ? "ON" : "OFF");
			break;

		/* Get current status. Returns on of "PAUSE", "PLAYING" and "STOPPED". */
		case 19:
			strncpy(reply, PLAYBACK_STATUS, BUFSIZE);
			break;

		/* Detach from network interface. */
		case 20:
			return 1;
	}

	return 0;
}


void parse_volume(const char * cmd) {
	char sign = 0;
	int new_volume;

	if(sscanf(cmd, "volume %1[+-]%d", & sign, & new_volume) == 2) {
		if(sign == '-')
			set_volume(volume - new_volume);
		else if(sign == '+')
			set_volume(volume + new_volume);
	}

	/* Allow percentual volume (1-100%). */
	else if(sscanf(cmd, "volume %%%d", & new_volume)) {
		set_volume(new_volume * 0.64);
	}

	/* Allow absolute volume (0-64). */
	else if(sscanf(cmd, "volume %d", & new_volume) == 1) {
		set_volume(new_volume);
	}
}
