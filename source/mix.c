
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/soundcard.h>

#include "mix.h"

#define VOL 0
#define PCM 4


#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

signed adjust(signed deviation) {
	signed fd = open("/dev/mixer", O_RDONLY | O_NONBLOCK), volume;

	if(fd == -1) {
		fprintf(stderr, "Can't open mixer device. %s.\n", strerror(errno));
		return -1;
	}
	else {
		/* Read volume. */
		ioctl(fd, MIXER_READ(PCM), & volume);

		volume += deviation;
		
		if(volume < 0)
			volume = 0;
		else if(volume > 0xFFFF)
			volume = 0xFFFF;

		/* Write volume. */
		ioctl(fd, MIXER_WRITE(PCM), & volume);

		return volume;
	}
}
