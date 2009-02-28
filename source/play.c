/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).

	Some code of this is shamelessly stolen from the libmad-example
	player minimad by Underbit Technologies.
*/

#define _GNU_SOURCE

#include <mad.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef LIBAO
#include <ao/ao.h>
#else

#if (defined(__NetBSD__) || defined(__OpenBSD__))
#include <soundcard.h>
#endif
#ifdef __FreeBSD__
#include <sys/soundcard.h>
#endif
#ifdef __linux__
#include <linux/soundcard.h>
#endif

#endif

#include "settings.h"
#include "pipe.h"
#include "play.h"
#include "interface.h"
#include "globals.h"

struct stream {
	FILE * streamfd;
#ifdef LIBAO
	int driver_id;
	ao_device * device;
	ao_sample_format fmt;
#else
	int audiofd;
#endif
	pid_t parent;

	FILE * dump;
	char * path;
};

#define BUFSIZE (32*1024)

static enum mad_flow input(void *, struct mad_stream *);
static enum mad_flow output(void *, const struct mad_header *, struct mad_pcm *);
inline signed scale(mad_fixed_t);

int killed = 0;

static void sighand(int);

/*
 * Taken from *BSD code
 * (usr.bin/patch/mkpath.c)
 */
int
mkpath(char *path)
{
	struct stat sb;
	char *slash;
	int done = 0;

	slash = path;

	while (!done) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		done = (*slash == '\0');
		*slash = '\0';

		if (stat(path, &sb)) {
			if (errno != ENOENT || (mkdir(path, 0777) &&
			    errno != EEXIST))
				return (-1);
		} else if (!S_ISDIR(sb.st_mode))
			return (-1);

		*slash = '/';
	}

	return (0);
}

int playback(FILE * streamfd) {
	const char * freetrack = NULL;

	killed = 0;
	signal(SIGUSR1, sighand);

	if(!haskey(& rc, "extern")) {
		struct stream data;
		struct mad_decoder dec;

#ifdef LIBAO
		static int ao_initialized = 0;

		if(!ao_initialized) {
			ao_initialize();
			ao_initialized = !0;
		}
#else
		unsigned arg;
		int fd;
#endif

		memset(& data, 0, sizeof(struct stream));

		data.streamfd = streamfd;
		data.parent = getppid();

#ifdef LIBAO
		data.driver_id = ao_default_driver_id();

		if(-1 == data.driver_id) {
			fputs("Unable to find any usable output device!\n", stderr);
			return 0;
		}

		data.fmt.bits = 16;
		data.fmt.rate = 44100;
		data.fmt.channels = 2;
		data.fmt.byte_format = AO_FMT_NATIVE;
		data.device = ao_open_live(data.driver_id,&data.fmt,NULL);

		if(NULL == data.device) {
			fprintf(stderr, "Unable to open device. %s.\n", strerror(errno));
			return 0;
		}
#else
		data.audiofd = fd = open(value(& rc, "device"), O_WRONLY);

		if(-1 == data.audiofd) {
			fprintf(
					stderr, "Couldn't open %s! %s.\n",
					value(& rc, "device"), strerror(errno)
			);
			return 0;
		}

		arg = 16; /* 16 bits */
		ioctl(data.audiofd, SOUND_PCM_WRITE_BITS, & arg);
#endif

		freetrack = value(& track, "freeTrackURL");

		if(freetrack && strlen(freetrack) > 0 && haskey(& rc, "download")) {
			char * dnam;
			int rv;

			data.path = strdup(meta(value(& rc, "download"), M_RELAXPATH, & track));
			assert(data.path != NULL);

			dnam = strdup(data.path);
			rv = dnam ? mkpath(dirname(dnam)) : -1;
			free(dnam);

			data.dump = (rv == 0) ? fopen(data.path, "w") : NULL;

			if(!data.dump)
				fprintf(stderr, "Can't write download to %s.\n", data.path);
		}

		mad_decoder_init(& dec, & data, input, NULL, NULL, output, NULL, NULL);
		mad_decoder_run(& dec, MAD_DECODER_MODE_SYNC);
#ifndef LIBAO
		close(fd);
#endif
		mad_decoder_finish(& dec);

		if(data.dump) {
			fclose(data.dump);

			if(killed)
				unlink(data.path);

			free(data.path);
		}

	} else {
		pid_t ppid = getppid(), cpid = 0;
		const char * cmd = meta(value(& rc, "extern"), 0, & track);
		FILE * ext = openpipe(cmd, & cpid);
		unsigned char * buf;

		if(!ext) {
			fprintf(stderr, "Failed to execute external player (%s). %s.\n",
					cmd, strerror(errno));
			return 0;
		}

		if(!(buf = calloc(BUFSIZE + 1, sizeof(unsigned char)))) {
			fputs("Couldn't allocate enough memory for input buffer.\n", stderr);
			fclose(ext);
			return 0;
		}

		while(!feof(streamfd)) {
			signed nbyte = fread(buf, sizeof(unsigned char), BUFSIZE, streamfd);

			if(nbyte > 0) {
				fwrite(buf, sizeof(unsigned char), nbyte, ext);
				fflush(ext);
			}

			if(kill(ppid, 0) == -1 && errno == ESRCH)
				break;

			if(killed)
				break;
		}

		free(buf);
		fclose(ext);
	}

	return !0;
}

static enum mad_flow input(void * data, struct mad_stream * stream) {
	static unsigned nbyte = 0;
	static int preload = 0;
	static unsigned char buf[BUFSIZE + 1] = { 0 };

	struct stream * ptr = (struct stream *) data;
	unsigned remnbyte = 0;

	if(feof(ptr->streamfd))
		return MAD_FLOW_STOP;

	if(stream->next_frame) {
		remnbyte = (unsigned) (& buf[nbyte] - stream->next_frame);
		memmove(buf, stream->next_frame, remnbyte);
	}

	if(preload) {
		nbyte = read(fileno(ptr->streamfd), buf + remnbyte, BUFSIZE - remnbyte);
		if(ptr->dump)
			fwrite(buf + remnbyte, sizeof(buf[0]), nbyte, ptr->dump);
	}
	else {
		while(nbyte < BUFSIZE) {
			int retval = read(fileno(ptr->streamfd), buf + nbyte, BUFSIZE - nbyte);
			if(retval <= 0)
				break;

			if(ptr->dump)
				fwrite(buf + nbyte, sizeof(buf[0]), retval, ptr->dump);

			nbyte += retval;
		}
		preload = !0;
	}
	
	if(!nbyte)
		return MAD_FLOW_STOP;

	nbyte += remnbyte;

	mad_stream_buffer(stream, (unsigned char *) buf, nbyte);


	if(kill(ptr->parent, 0) == -1 && errno == ESRCH) {
		fclose(ptr->streamfd);
		killed = !0;
		return MAD_FLOW_STOP;
	}

	if(killed)
		return MAD_FLOW_STOP;

	return MAD_FLOW_CONTINUE;
}

#ifdef LIBAO
static enum mad_flow output(
		void * data,
		const struct mad_header * head,
		struct mad_pcm * pcm) {
	struct stream * ptr = (struct stream *) data;

	unsigned nchan = pcm->channels, rate = pcm->samplerate;
	register unsigned nsample = pcm->length;
	mad_fixed_t * left = pcm->samples[0], * right = pcm->samples[1];
	char *stream, *stream_ptr;

	head = NULL;

	if((signed) rate != ptr->fmt.rate || (signed) nchan != ptr->fmt.channels) {
		ptr->fmt.rate = rate;
		ptr->fmt.channels = nchan;
		if(ptr->device != NULL)
			ao_close(ptr->device);
		ptr->device = ao_open_live(ptr->driver_id, & ptr->fmt, NULL);

		if(NULL == ptr->device) {
			fprintf(stderr, "Unable to open device. %s.\n", strerror(errno));
			return MAD_FLOW_BREAK;
		}
	}

	stream_ptr = stream = malloc(pcm->length * (pcm->channels == 2 ? 4 : 2));

	assert(stream != NULL);
	
	while(nsample--) {
		signed int sample;

		sample = scale(* left++);
		/* to byteswap or not to byteswap? */
#ifdef WORDS_BIGENDIAN
		*stream_ptr++ = (sample >> 8) & 0xFF;
		*stream_ptr++ = (sample & 0xFF);
#else
		*stream_ptr++ = (sample & 0xFF);
		*stream_ptr++ = (sample >> 8) & 0xFF;
#endif

		if(nchan == 2) {
			sample = scale(* right++);
#ifdef WORDS_BIGENDIAN
			*stream_ptr++ = (sample >> 8) & 0xFF;
			*stream_ptr++ = (sample & 0xFF);
#else
			*stream_ptr++ = (sample & 0xFF);
			*stream_ptr++ = (sample >> 8) & 0xFF;
#endif
		}
	}
	ao_play(ptr->device, stream, pcm->length * (pcm->channels == 2 ? 4 : 2));
	free(stream);

	if(killed)
		return MAD_FLOW_STOP;

	return MAD_FLOW_CONTINUE;
}
#else
static enum mad_flow output(
		void * data,
		const struct mad_header * head,
		struct mad_pcm * pcm) {
	struct stream * ptr = (struct stream *) data;

	unsigned nchan = pcm->channels, rate = pcm->samplerate, arg;
	register unsigned nsample = pcm->length;
	mad_fixed_t * left = pcm->samples[0], * right = pcm->samples[1];

	head = NULL;

	arg = rate;
	ioctl(ptr->audiofd, SOUND_PCM_WRITE_RATE, & arg);
	arg = nchan;
	ioctl(ptr->audiofd, SOUND_PCM_WRITE_CHANNELS, & arg);

	while(nsample--) {
		signed sample;
		unsigned char word[2];

		sample = scale(* left++);
		word[0] = (unsigned char) (sample & 0xFF);
		word[1] = (unsigned char) ((sample >> 8) & 0xFF);
		write(ptr->audiofd, word, 2);

		if(nchan == 2) {
			sample = scale(* right++);

			word[0] = (unsigned char) (sample & 0xFF);
			word[1] = (unsigned char) ((sample >> 8) & 0xFF);

			write(ptr->audiofd, word, 2);
		}
	}

	if(killed)
		return MAD_FLOW_STOP;

	return MAD_FLOW_CONTINUE;
}
#endif

inline signed scale(register mad_fixed_t sample) {
	sample += (1L << (MAD_F_FRACBITS - 16));
	
	if(sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if(sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static void sighand(int sig) {
	if(sig == SIGUSR1)
		killed = !0;
}
