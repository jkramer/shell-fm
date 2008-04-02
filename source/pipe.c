/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#define _GNU_SOURCE

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <errno.h>
#include <string.h>

#include "interface.h"
#include "pipe.h"

FILE * openpipe(const char * cmd, pid_t * cpid) {
	pid_t pid;
	int fd[2] = { 0 };

	assert(cmd);

	if(-1 == pipe(fd)) {
		fprintf(stderr, "Couldn't create pipe. %s.\n", strerror(errno));
		return NULL;
	}

	pid = fork();

	if(pid == -1) {
		fprintf(stderr, "Fork failed. %s.\n", strerror(errno));
		close(fd[1]);
		close(fd[0]);
		return NULL;
	}

	if(!pid) {
		close(fd[1]);
		dup2(fd[0], 0);

		execl("/bin/sh", "sh", "-c", cmd, NULL);
	}

	if(cpid != NULL)
		* cpid = pid;

	return fdopen(fd[1], "w");
}
