CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -Wall -Wextra -pthread

.PHONY: all clean

all: lookup

lookup: lookup.o queue.o util.o
	$(CC) $(LFLAGS) $^ -o $@

lookup.o: lookup.c lookup.h
	$(CC) $(CFLAGS) $<

clean:
	rm -f lookup
	rm -f *.o
	rm -f *~
	rm -f results.txt
