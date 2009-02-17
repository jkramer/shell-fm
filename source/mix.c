/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).
*/

#include <fcntl.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#if (defined(__NetBSD__) || defined(__OpenBSD__))
#include <soundcard.h>
#endif
#ifdef __FreeBSD__
#include <sys/soundcard.h>
#endif
#ifdef __linux__
#include <linux/soundcard.h>
#endif

#include "mix.h"

signed adjust(signed deviation, int device) {
	signed fd = open("/dev/mixer", O_RDONLY | O_NONBLOCK), volume;

	if(fd == -1) {
		fprintf(stderr, "Can't open mixer device. %s.\n", strerror(errno));
		return -1;
	}
	else {
		/* Read volume. */
		ioctl(fd, MIXER_READ(device), & volume);

		volume += deviation;
		
		if(volume < 0)
			volume = 0;
		else if(volume > 0xFFFF)
			volume = 0xFFFF;

		/* Write volume. */
		ioctl(fd, MIXER_WRITE(device), & volume);

		close(fd);

		return volume;
	}
}
