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
	fprintf(stderr, "usage: %s\n",
	        argv0 ? argv0 : "john-blund");
	exit(1);
}


void
fill_buffer(UTYPE *buffer, double volume)
{
#define GENERATE_TONE(tone)  sin(2 * M_PI * ((double)i / (SAMPLE_RATE / (tone))))

	size_t i, j = 0;

	for (i = 0; i < N; i++, j += 2) {
		buffer[j + 0] = GENERATE_TONE(370) * SMAX * volume - SMIN;
		buffer[j + 1] = GENERATE_TONE(360) * SMAX * volume - SMIN;
	}
}


int
main(int argc, char *argv[])
{
	UTYPE buffer[N * 2];
	int r;
	snd_pcm_t *sound_handle;
	snd_pcm_sframes_t frames;
	int n = 3;
	double volume = 0.25;

	ARGBEGIN {
	case 'v':
		volume *= strtod(EARGF(usage()), NULL);
		break;
	default:
		usage();
	} ARGEND;

	if (argc)
		usage();

	/* Generate audio. */
	fill_buffer(buffer, volume);

	/* Set up audio. */
	if (r = snd_pcm_open(&sound_handle, "default", SND_PCM_STREAM_PLAYBACK, 0), r < 0)
		return fprintf(stderr, "%s: snd_pcm_open: %s\n", *argv, snd_strerror(r)), 1;
	if (sound_handle == NULL)
		perror("snd_pcm_open");

	/* Configure audio. */
	r = snd_pcm_set_params(sound_handle, FORMAT, SND_PCM_ACCESS_RW_INTERLEAVED, 2 /* channels */,
	                       SAMPLE_RATE, 1 /* allow resampling? */, LATENCY);
	if (r < 0)
		return fprintf(stderr, "%s: snd_pcm_set_params: %s\n", argv0, snd_strerror(r)), 1;

	while (n--) {
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
