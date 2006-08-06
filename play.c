
/*
	vim:syntax=c tabstop=2 shiftwidth=2

	Shell.FM - play.c
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

#include <sys/ioctl.h>
#ifdef __HAVE_LIBAO__
#include <ao/ao.h>
#else
#include <linux/soundcard.h>
#endif

struct stream {
	FILE * streamfd;
#ifdef __HAVE_LIBAO__
	int driver_id;
	ao_device *device;
	ao_sample_format fmt;
#else
	int audiofd;
#endif
	pid_t parent;
};

#define BUFSIZE 40960

static enum mad_flow input(void *, struct mad_stream *);
static enum mad_flow output(void *, const struct mad_header *, struct mad_pcm *);
signed scale(mad_fixed_t);

void * findsync(register unsigned char *, unsigned);

#ifdef __HAVE_LIBAO__
void playback(FILE * streamfd) {
	static int ao_initialized = 0;
	struct stream data;
	struct mad_decoder dec;

	if(!ao_initialized) {
		ao_initialize();
		ao_initialized = 1;
	}

	memset(& data, 0, sizeof(struct stream));
	
	data.streamfd = streamfd;
	data.driver_id = ao_default_driver_id();
	data.parent = getppid();

	if(-1 == data.driver_id) {
		fprintf(stderr, "Unable to find any usable output device!\n");
		return;
	}

	data.fmt.bits = 16;
	data.fmt.rate = 44100;
	data.fmt.channels = 2;
	data.fmt.byte_format = AO_FMT_BIG;
	data.device = ao_open_live(data.driver_id,&data.fmt,NULL);

	if(NULL == data.device) {
		fprintf(stderr, "Unable to open device. %s.\n", strerror(errno));
		return;
	}

	mad_decoder_init(& dec, & data, input, NULL, NULL, output, NULL, NULL);
	mad_decoder_run(& dec, MAD_DECODER_MODE_SYNC);
	mad_decoder_finish(& dec);

	fprintf(stderr, "Reached end of stream.\n");
}
#else
void playback(FILE * streamfd) {
	struct stream data;
	struct mad_decoder dec;
	unsigned arg;

	memset(& data, 0, sizeof(struct stream));
	
	data.streamfd = streamfd;
	data.audiofd = open("/dev/audio", O_WRONLY);
	data.parent = getppid();

	if(-1 == data.audiofd) {
		fprintf(stderr, "Couldn't open /dev/dsp! %s.\n", strerror(errno));
		return;
	}

	arg = 16; /* 16 bits */
	ioctl(data.audiofd, SOUND_PCM_WRITE_BITS, & arg);

	mad_decoder_init(& dec, & data, input, NULL, NULL, output, NULL, NULL);
	mad_decoder_run(& dec, MAD_DECODER_MODE_SYNC);
	mad_decoder_finish(& dec);

	fprintf(stderr, "Reached end of stream.\n");
}
#endif

static enum mad_flow input(void * data, struct mad_stream * stream) {
	static unsigned nbyte = 0;
	static unsigned char buf[BUFSIZE + 1] = { 0 };

	struct stream * ptr = (struct stream *) data;
	unsigned remnbyte = 0;
	unsigned char * syncptr;

	if(feof(ptr->streamfd))
		return MAD_FLOW_STOP;

	if(stream->next_frame) {
		remnbyte = (unsigned) (& buf[nbyte] - stream->next_frame);
		memmove(buf, stream->next_frame, remnbyte);
	}

	nbyte = read(fileno(ptr->streamfd), buf + remnbyte, BUFSIZE - remnbyte);
	if(!nbyte)
		return MAD_FLOW_STOP;

	nbyte += remnbyte;

	if((syncptr = findsync(buf, nbyte)) != NULL) {
		remnbyte = ((unsigned) (& buf[nbyte] - syncptr)) - 4;
		memmove(syncptr, syncptr + 4, remnbyte);
		kill(ptr->parent, SIGUSR1);
		nbyte -= 4;
	}

	mad_stream_buffer(stream, (unsigned char *) buf, nbyte);
	return MAD_FLOW_CONTINUE;
}

#ifdef __HAVE_LIBAO__
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
	
	while(nsample--) {
		signed int sample;

		sample = scale(* left++);
		*stream_ptr++ = (sample & 0xFF);
		*stream_ptr++ = (sample >> 8) & 0xFF;

		if(nchan == 2) {
			sample = scale(* right++);
			*stream_ptr++ = (sample & 0xFF);
			*stream_ptr++ = (sample >> 8) & 0xFF;
		}
	}
	ao_play(ptr->device, stream, pcm->length * (pcm->channels == 2 ? 4 : 2));
	free(stream);

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
		word[0] = (sample & 0xFF);
		word[1] = (sample >> 8) & 0xFF;
		write(ptr->audiofd, word, 2);

		if(nchan == 2) {
			sample = scale(* right++);
			word[0] = (sample & 0xFF);
			word[1] = (sample >> 8) & 0xFF;
			write(ptr->audiofd, word, 2);
		}
	}

	return MAD_FLOW_CONTINUE;
}
#endif

signed scale(register mad_fixed_t sample) {
	sample += (1L << (MAD_F_FRACBITS - 16));
	
	if(sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if(sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

void * findsync(register unsigned char * ptr, unsigned size) {
	register const unsigned char * needle = (unsigned char *) "SYNC";
	register unsigned char * end = ptr + size - 4;
	while(ptr < end) {
		if((* (unsigned *) ptr) == * (unsigned *) needle)
			return ptr;
		++ptr;
	}
	return NULL;
}
