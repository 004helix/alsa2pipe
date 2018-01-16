CC = gcc
LD = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -lasound

EXE = alsa2pipe

$(EXE): $(EXE).c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(EXE)
