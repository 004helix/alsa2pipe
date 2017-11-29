CC = gcc
LD = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -lasound

EXE = pulse-alsa2pipe

$(EXE): $(EXE).c
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(EXE)
