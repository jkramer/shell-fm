
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
#include <linux/soundcard.h>

struct stream {
	FILE * streamfd;
	int audiofd;
	pid_t parent;
};

#define BUFSIZE 40960

void handle(int);

static enum mad_flow input(void *, struct mad_stream *);
static enum mad_flow output(void *, const struct mad_header *, struct mad_pcm *);
signed scale(mad_fixed_t);

void * findsync(register unsigned char *, unsigned);

void * playback(FILE * streamfd) {
	struct stream data;
	struct mad_decoder dec;
	signed res;
	unsigned arg;

	signal(SIGCHLD, handle);
	signal(SIGTERM, handle);
	signal(SIGINT, handle);
	signal(SIGQUIT, handle);
	signal(SIGABRT, handle);
	signal(SIGSEGV, handle);
	signal(SIGALRM, handle);
	signal(SIGPIPE, handle);
	signal(SIGSTOP, handle);
	signal(SIGCONT, handle);
	signal(SIGILL, handle);
	
	memset(& data, 0, sizeof(struct stream));
	
	data.streamfd = streamfd;
	data.audiofd = open("/dev/dsp", O_WRONLY);
	data.parent = getppid();

	if(-1 == data.audiofd) {
		fprintf(stderr, "Couldn't open /dev/dsp! %s.\n", strerror(errno));
		return NULL;
	}

	arg = 16; /* 16 bits */
	ioctl(data.audiofd, SOUND_PCM_WRITE_BITS, & arg);

	mad_decoder_init(& dec, & data, input, NULL, NULL, output, NULL, NULL);
	res = mad_decoder_run(& dec, MAD_DECODER_MODE_SYNC);
	mad_decoder_finish(& dec);

	fprintf(stderr, "Reached end of stream.\n");

	return (void *) res;
}

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

void handle(int sig) {
	fprintf(stderr, "CAUGHT SIGNAL %d.\n", sig);
	fflush(stderr);
}
