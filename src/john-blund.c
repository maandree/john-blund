/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <math.h> /* -lm */
#include <alsa/asoundlib.h> /* dep: alsa-lib (pkg-config alsa) */

#include "arg.h"


#define SAMPLE_RATE  52000   /* Hz */
#define LATENCY      100000  /* µs */

#define UTYPE        uint32_t
#define SMIN         INT32_MIN
#define SMAX         INT32_MAX
#define FORMAT       SND_PCM_FORMAT_U32

#define DURATION     1000    /* ms */
#define N            (SAMPLE_RATE / 1000 * DURATION)


char *argv0;


void
usage(void)
{
	fprintf(stderr, "usage: %s [-v volume] [-t base-tone] [-g gap] [-m]\n",
	        argv0 ? argv0 : "john-blund");
	exit(1);
}


void
fill_buffer(UTYPE *buffer, double volume, long int tone1, long int tone2, int ch)
{
#define GEN_TONE(tone)  sin(2 * M_PI * ((double)(i + last) / (SAMPLE_RATE / (tone))))

	size_t i, j = 0;
	static size_t last = 0;

	if (ch == 2) {
		for (i = 0; i < N; i++, j += 2) {
			buffer[j + 0] = GEN_TONE(tone1) * SMAX * volume - SMIN;
			buffer[j + 1] = GEN_TONE(tone2) * SMAX * volume - SMIN;
		}
	} else {
		for (i = 0; i < N; i++)
			buffer[i] = (GEN_TONE(tone1) + GEN_TONE(tone2)) * SMAX / 2 * volume - SMIN;
	}

	last += N;
	last %= tone1 * tone2 * 1000;
}


int
main(int argc, char *argv[])
{
	UTYPE *buffer;
	int r, channels = 2;
	snd_pcm_t *sound_handle;
	snd_pcm_sframes_t frames;
	double volume = 0.25;
	long int tone1 = 100, tone2 = 10;

	ARGBEGIN {
	case 'v':
		volume *= strtod(EARGF(usage()), NULL);
		break;
	case 't':
		tone1 = strtol(EARGF(usage()), NULL, 10);
		break;
	case 'g':
		tone2 = strtol(EARGF(usage()), NULL, 10); /* gap from tone1 */
		break;
	case 'm':
		channels = 1;
		break;
	default:
		usage();
	} ARGEND;

	if (argc)
		usage();

	tone2 += tone1;
	buffer = malloc(N * channels * sizeof(*buffer));
	if (!buffer)
		return perror("malloc"), 1;

	/* Set up audio. */
	if (r = snd_pcm_open(&sound_handle, "default", SND_PCM_STREAM_PLAYBACK, 0), r < 0)
		return fprintf(stderr, "%s: snd_pcm_open: %s\n", *argv, snd_strerror(r)), 1;
	if (sound_handle == NULL)
		return perror("snd_pcm_open"), 1;

	/* Configure audio. */
	r = snd_pcm_set_params(sound_handle, FORMAT, SND_PCM_ACCESS_RW_INTERLEAVED, channels,
	                       SAMPLE_RATE, 1 /* allow resampling? */, LATENCY);
	if (r < 0)
		return fprintf(stderr, "%s: snd_pcm_set_params: %s\n", argv0, snd_strerror(r)), 1;

	/* Playback. */
	for (;;) {
		fill_buffer(buffer, volume, tone1, tone2, channels);
		r = frames = snd_pcm_writei(sound_handle, buffer, N);
		if (frames < 0)
			r = frames = snd_pcm_recover(sound_handle, r, 0 /* do not print error reason? */);
		if (r < 0)
			return fprintf(stderr, "%s: snd_pcm_writei: %s\n", argv0, snd_strerror(r)), 1;
		if ((r > 0) && ((size_t)r < N))
			printf("%s: short write: expected %zu, wrote %zu\n", argv0, N, (size_t)frames);
	}

	snd_pcm_close(sound_handle);
	return 0;
}
