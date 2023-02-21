CC=gcc -D_FILE_OFFSET_BITS=64 -Wall -g -DFUSE_USE_VERSION=25 -lm

tyrant: tyrant.c tyrant_help.c disk.c
	$(CC) tyrant.c tyrant_help.c disk.c -o tyrantfs `pkg-config fuse --cflags --libs`

tyrant.o: tyrant.c
	$(CC) -c tyrant.c

tyrant_help.o: tyrant_help.c
	$(CC) -c tyrant_help.c

disk.o: disk.c
	$(CC) -c disk.c

clean:
	rm -r *.o