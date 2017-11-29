/*
 *  Copyright (C) 2017 Raman Shyshniou <rommer@ibuffed.com>
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <alsa/asoundlib.h>


char *onconnect = NULL;
char *ondisconnect = NULL;


void runhook(char *prog)
{
    char *argv[2];
    pid_t pid = fork();

    switch (pid) {
        case -1:
            fprintf(stderr, "exec failed, fork: %s\n", strerror(errno));
            return;
        case 0:
            argv[0] = prog;
            argv[1] = NULL;
            execvp(prog, argv);
            fprintf(stderr, "execvp failed: %s\n", strerror(errno));
            exit(1);
        break;
    }
}


void run(snd_pcm_t *handle, int frames,
         char *buffer, size_t bufsize, int pipefd)
{
    int size, connected = 0;
    ssize_t rv;

    for (;;) {
        // read audio data from alsa
        size = snd_pcm_readi(handle, buffer, frames);

        if (size != frames) {
            // error: disconnect if connected
            if (connected) {
                fprintf(stderr, "ALSA source disconnected (%d/%d)\n",
                        size, frames);
                connected = 0;

                // run disconnect hook
                if (ondisconnect)
                    runhook(ondisconnect);
            }

            sleep(1);
            continue;
        }

        if (!connected) {
            fprintf(stderr, "ALSA source connected\n");

            // run connect hook
            if (onconnect)
                runhook(onconnect);

            connected++;
        }

        // send audio data to pulseaudio
        rv = write(pipefd, buffer, bufsize);
        if (rv < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // report override can be very noisy if source suspended
                // fprintf(stderr, "pipe overrun: size %u\n", (unsigned) bufsize);
                continue;
            }
            perror("pipe write");
            return;
        }
    }
}


int main(int argc, char *argv[])
{
    char *buffer;
    int pipefd, err, i;
    unsigned frames = 128;
    unsigned rate, channels;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_format_t format;
    char sformat[32];
    size_t bufsize;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    // check command line arguments
    if (argc < 4) {
        fprintf(stderr,
                "Usage:\n %s <device> <format> <pipe>"
                " [exec-on-connect] [exec-on-disconnect]\n"
                "  format: <sample-format:sample-rate:channels[:buffer]>\n"
                "    sample-format: u8, s8, s16le, s16be\n"
                "                   s24le, s24be, s32le, s32be\n"
                "    sample-rate: 48000, 44100, ...\n"
                "    channels: 4, 2, 1, ...\n"
                "    buffer: buffer duration is # frames (128)\n",
                argv[0]);
        return 1;
    }

    bufsize = 0;
    buffer = strdup(argv[2]);
    for (i = 0; buffer[i]; i++) {
        bufsize++;
        if (buffer[i] == ':')
            buffer[i] = ' ';
    }

    if (bufsize >= sizeof(sformat)) {
        fprintf(stderr, "format option too long\n");
        return 1;
    }

    if (sscanf(buffer, "%s %u %u %u",
               sformat, &rate, &channels, &frames) < 3) {
        fprintf(stderr, "unknown format: %s\n", argv[2]);
        free(buffer);
        return 1;
    }

    free(buffer);

    if (!strcmp(sformat, "s8")) {
        format = SND_PCM_FORMAT_S8;
    } else
    if (!strcmp(sformat, "u8")) {
        format = SND_PCM_FORMAT_U8;
    } else
    if (!strcmp(sformat, "s16le")) {
        format = SND_PCM_FORMAT_S16_LE;
    } else
    if (!strcmp(sformat, "s16be")) {
        format = SND_PCM_FORMAT_S16_BE;
    } else
    if (!strcmp(sformat, "s24le")) {
        format = SND_PCM_FORMAT_S24_LE;
    } else
    if (!strcmp(sformat, "s24be")) {
        format = SND_PCM_FORMAT_S24_BE;
    } else
    if (!strcmp(sformat, "s32le")) {
        format = SND_PCM_FORMAT_S32_LE;
    } else
    if (!strcmp(sformat, "s32be")) {
        format = SND_PCM_FORMAT_S32_BE;
    } else {
        fprintf(stderr, "unknown frame format: %s\n", sformat);
        return 1;
    }

    pipefd = open(argv[3], O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (pipefd < 0) {
        perror("pipe open");
        return 1;
    }

    // setup connect/disconnect handlers
    if (argc > 4)
        onconnect = strdup(argv[4]);

    if (argc > 5)
        ondisconnect = strdup(argv[5]);

    // open alsa device
    if ((err = snd_pcm_open(&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                argv[1], snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
        fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
                snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot set access type (%s)\n",
                snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params, format)) < 0) {
        fprintf(stderr, "cannot set sample format (%s)\n",
                snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0)) < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n",
                snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels)) < 0) {
        fprintf(stderr,
                "cannot set channel count (%s)\n",
                snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
        fprintf(stderr,
                "cannot set parameters (%s)\n",
                snd_strerror(err));
        return 1;
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(capture_handle)) < 0) {
        fprintf(stderr,
                "cannot prepare audio interface for use (%s)\n",
                snd_strerror(err));
        return 1;
    }

    bufsize = frames * channels * snd_pcm_format_width(format) / 8;
    if ((buffer = malloc(bufsize)) == NULL) {
        fprintf(stderr, "cannot allocate memory\n");
        return 1;
    }

    // run
    run(capture_handle, frames, buffer, bufsize, pipefd);

    free(buffer);

    snd_pcm_close(capture_handle);

    return 0;
}
