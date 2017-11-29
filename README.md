# ALSA recorder

PulseAudio's alsa-source module always is in running state if it gets
errors from capturing device. When IEC-958 (S/PDIF) input was selected
on device it can generate errors when a cable was disconnected or
remote device was turned off.

This program provide an ability to suspend/unsuspend source module
when it detects errors from alsa capturing device.

# Usage:

```
Usage:
 alsa2pipe <device> <format> <pipe> [exec-on-connect] [exec-on-disconnect]
  format: <sample-format:sample-rate:channels[:buffer]>
    sample-format: u8, s8, s16le, s16be
                   s24le, s24be, s32le, s32be
    sample-rate: 48000, 44100, ...
    channels: 4, 2, 1, ...
    buffer: buffer duration is # frames (128)

```

# Example:

load pipe-source module instead of alsa-source in pulseaudio:
```
$ pactl load-module module-pipe-source source_name=spdif \
  file=/tmp/spdif.input format=s16le rate=48000 channels=2
```

suspend source module
```
$ pactl suspend-source spdif 1
```

create onconnect/ondisconnect scripts
```
$ echo -e '#!/bin/bash\npactl suspend-source spdif 1' > ~/bin/onconnect
$ echo -e '#!/bin/bash\npactl suspend-source spdif 0' > ~/bin/ondisconnect
$ chmod +x ~/bin/onconnect ~/bin/ondisconnect
```

start alsa2pipe
```
$ alsa2pipe hw:0,0 s16le:48000:2 /tmp/spdif.input onconnect ondisconnect
```
