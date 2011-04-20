/*
	Copyright (C) 2006 by Jonas Kramer
	Published under the terms of the GNU General Public License (GPL).

	Some code of this is shamelessly stolen from the libmad-example
	player minimad by Underbit Technologies.
*/

#define _GNU_SOURCE

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
#include <sys/select.h>

#ifndef EXTERN_ONLY
#include <mad.h>

#ifdef LIBAO
#include <ao/ao.h>
#else

#if (defined(__NetBSD__) || defined(__OpenBSD__))
#include <soundcard.h>
#endif
#if (defined(__FreeBSD__) || defined(__FreeBSD_kernel__))
#include <sys/soundcard.h>
#endif
#ifdef __linux__
#include <linux/soundcard.h>
#endif

#endif

#ifdef TAGLIB
#include <taglib/tag_c.h>
#endif
#endif

#include "settings.h"
#include "pipe.h"
#include "play.h"
#include "interface.h"
#include "globals.h"
#include "util.h"
#include "http.h"

#ifndef EXTERN_ONLY
struct stream {
	struct content_handle stream;
#ifdef LIBAO
	int driver_id;
	ao_device * device;
	ao_sample_format fmt;
#else
	int audiofd;
#endif
	pid_t parent;

	FILE * dump;
	char * finpath;             // final destination
	char * tmppath;             // streaming to file
	int pipefd;

	int timeout;
	int preload;
};
#endif

#define BUFSIZE (32*1024)

#ifndef EXTERN_ONLY
static enum mad_flow input(void *, struct mad_stream *);
static enum mad_flow output(void *, const struct mad_header *, struct mad_pcm *);
inline signed scale(mad_fixed_t);
static int timed_read(struct content_handle *, unsigned char *, int, int);
#endif

int killed = 0;

unsigned char volume = MAX_VOLUME;

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

int playback(struct content_handle handle, int pipefd) {
	killed = 0;
	signal(SIGUSR1, sighand);

#ifndef EXTERN_ONLY
	if(!haskey(& rc, "extern")) {
		const char * freetrack = NULL;

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

		/*
			Check if there's a stream timeout configured and set it up for timed
			reads later.
		*/
		data.timeout = -1;
		if(haskey(& rc, "stream-timeout")) {
			const char * timeout = value(& rc, "stream-timeout");
			data.timeout = atoi(timeout);

			if(data.timeout <= 0) {
				if(data.timeout < 0) 
					fputs("Invalid stream-timeout.\n", stderr);

				data.timeout = -1;
			}
		}


		data.stream = handle;
		data.parent = getppid();
		data.pipefd = pipefd;
		fcntl(pipefd, F_SETFL, O_NONBLOCK);

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

			data.finpath = strdup(meta(value(& rc, "download"), M_RELAXPATH, & track));
			assert(data.finpath != NULL);

			data.tmppath = strjoin("", data.finpath, ".streaming", NULL);
			assert(data.tmppath != NULL);

			dnam = strdup(data.tmppath);
			rv = dnam ? mkpath(dirname(dnam)) : -1;
			free(dnam);

			if(access(data.tmppath, R_OK) == -1) {
				data.dump = (rv == 0) ? fopen(data.tmppath, "w") : NULL;

				if(!data.dump)
					fprintf(stderr, "Can't write download to %s.\n", data.tmppath);
			}
			else {
				data.dump = NULL;
			}
		}

		mad_decoder_init(& dec, & data, input, NULL, NULL, output, NULL, NULL);
		mad_decoder_run(& dec, MAD_DECODER_MODE_SYNC);
#ifndef LIBAO
		close(fd);
#endif
		mad_decoder_finish(& dec);

		if(data.dump) {
			fclose(data.dump);

			if(killed) {
				unlink(data.tmppath);
			} else {
				int rv;
#ifdef TAGLIB
				TagLib_File *tagme = taglib_file_new_type(data.tmppath, TagLib_File_MPEG);
				if(tagme != NULL) {
					TagLib_Tag *tag = taglib_file_tag(tagme);
					taglib_tag_set_title(tag, value(&track, "title"));
					taglib_tag_set_artist(tag, value(&track, "creator"));
					taglib_tag_set_album(tag, value(&track, "album"));
					taglib_file_save(tagme);
					taglib_file_free(tagme);
				}
#endif
				if(haskey(& rc, "pp-cmd")) {
					const char *ppcmd = value(& rc, "pp-cmd");
					size_t ppcmdlen = strlen(ppcmd);
					char *path = shellescape(data.tmppath);
					assert(path != NULL);
					size_t pathlen = strlen(path);
					char *command = malloc(ppcmdlen + pathlen + 2);
					assert(command != NULL);
					memcpy(command, ppcmd, ppcmdlen);
					command[ppcmdlen] = ' ';
					memcpy(command + ppcmdlen + 1, path, pathlen);
					command[ppcmdlen + 1 + pathlen] = 0;
					run(command);
					free(path);
					free(command);
				}

				rv = rename(data.tmppath, data.finpath);
				if (rv == -1)
					fprintf(stderr, "Can't rename %s to %s\n",
							data.tmppath, data.finpath);
			}

			free(data.tmppath);
			free(data.finpath);
		}
	}
	else
#endif
	{
		pid_t ppid = getppid(), cpid = 0;
		const char * cmd = meta(value(& rc, "extern"), M_SHELLESC, & track);
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

		while(!feof(handle.fd)) {
			signed nbyte = receive(& handle, (char *) buf, BUFSIZE);

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

		waitpid(cpid, NULL, 0);
	}

	return !0;
}

static void sighand(int sig) {
	if(sig == SIGUSR1)
		killed = !0;
}

#ifndef EXTERN_ONLY
static enum mad_flow input(void * data, struct mad_stream * stream) {
	static unsigned char buf[BUFSIZE + 1] = { 0 };
	struct stream * ptr = (struct stream *) data;

	static int nbyte = 0;
	int remnbyte = 0;

	if(feof(ptr->stream.fd))
		return MAD_FLOW_STOP;

	if(stream->next_frame) {
		remnbyte = (unsigned) (& buf[nbyte] - stream->next_frame);
		memmove(buf, stream->next_frame, remnbyte);
	}

	if(ptr->preload) {
		nbyte = timed_read(& ptr->stream, buf + remnbyte, BUFSIZE - remnbyte, ptr->timeout);

		if(nbyte == -1) {
			fputs("Stream timed out.\n", stderr);
		}

		else if(ptr->dump)
			fwrite(buf + remnbyte, sizeof(buf[0]), nbyte, ptr->dump);
	}
	else {
		while(nbyte < BUFSIZE) {
			int retval = timed_read(& ptr->stream, buf + nbyte, BUFSIZE - nbyte, ptr->timeout);

			if(retval <= 0)
				break;

			if(ptr->dump)
				fwrite(buf + nbyte, sizeof(buf[0]), retval, ptr->dump);

			nbyte += retval;
		}

		if(!nbyte) {
			fputs("Stream timed out while trying to preload data.\n", stderr);
		}

		ptr->preload = !0;
	}
	
	if(nbyte <= 0)
		return MAD_FLOW_STOP;

	nbyte += remnbyte;

	mad_stream_buffer(stream, (unsigned char *) buf, nbyte);


	if(kill(ptr->parent, 0) == -1 && errno == ESRCH) {
		fclose(ptr->stream.fd);
		killed = !0;
		return MAD_FLOW_STOP;
	}

	if(killed)
		return MAD_FLOW_STOP;

	return MAD_FLOW_CONTINUE;
}

static void read_from_pipe(int pipefd) {
	char readchar;
	ssize_t readcount;

	while((readcount = read(pipefd, & readchar, 1)) > 0)
		volume = (int) readchar;
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
	read_from_pipe(ptr->pipefd);
	
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
	read_from_pipe(ptr->pipefd);

	arg = rate;
	ioctl(ptr->audiofd, SOUND_PCM_WRITE_RATE, & arg);
	arg = nchan;
	ioctl(ptr->audiofd, SOUND_PCM_WRITE_CHANNELS, & arg);

	unsigned char * words, * word;

	words = malloc(nsample * 4);
	assert(words != NULL);

	word = words;

	while(nsample--) {
		signed sample;

		sample = scale(* left++);
		word[0] = (unsigned char) (sample & 0xFF);
		word[1] = (unsigned char) ((sample >> 8) & 0xFF);
		word += 2;

		if(nchan == 2) {
			sample = scale(* right++);

			word[0] = (unsigned char) (sample & 0xFF);
			word[1] = (unsigned char) ((sample >> 8) & 0xFF);

			word += 2;
		}
	}

	write(ptr->audiofd, words, word - words);
	free(words);

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

	return (sample >> (MAD_F_FRACBITS + 1 - 16)) * volume / MAX_VOLUME;
}

static int timed_read(struct content_handle * handle, unsigned char * p, int count, int timeout) {
	fd_set fdset;
	struct timeval tv;
	struct timeval * tvp = & tv;
	int fd = fileno(handle->fd);

	debug("timed read with handle\n");

	FD_ZERO(& fdset);
	FD_SET(fd, & fdset);

	if(timeout <= 0) {
		tvp = NULL;
	}
	else {
		tv.tv_usec = 0;
		tv.tv_sec = timeout;
	}

	if(select(fd + 1, & fdset, NULL, NULL, tvp) > 0) {
		return receive(handle, (char *) p, count);
	}

	fprintf(stderr, "Track stream timed out (%d).\n", timeout);
	return -1;
}
#endif
