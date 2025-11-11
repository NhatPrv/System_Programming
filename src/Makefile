CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread

all: writer reader cleanup

writer: writer.c shared.h
	$(CC) $(CFLAGS) writer.c -o writer

reader: reader.c shared.h
	$(CC) $(CFLAGS) reader.c -o reader

cleanup: cleanup.c shared.h
	$(CC) -O2 cleanup.c -o cleanup

clean:
	rm -f writer reader cleanup
