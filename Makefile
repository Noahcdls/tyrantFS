CC=gcc -D_FILE_OFFSET_BITS=64 -Wall -g -DFUSE_USE_VERSION=25 -lm

all: tyrant mkfs

tyrant: tyrant.c tyrant_help.c disk.c
	$(CC) tyrant.c tyrant_help.c disk.c -o tyrantfs `pkg-config fuse --cflags --libs`	

mkfs: mkfs.c tyrant_help.c disk.c
	$(CC) mkfs.c tyrant_help.c disk.c -o mkfs `pkg-config fuse --cflags --libs`	


test_block: test_block.c disk.o
	$(CC) disk.o test_block.c -o test_block

tyrant.o: tyrant.c
	$(CC) -c tyrant.c

tyrant_help.o: tyrant_help.c
	$(CC) -c tyrant_help.c

disk.o: disk.c
	$(CC) -c disk.c

clean:
	rm -r *.o tyrantfs mkfs test_block
