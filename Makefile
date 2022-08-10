CC=gcc
CFLAGS=-Wall -Wextra -g
CLIBS=-lstrophe -lnotcurses
CODE=src/

all: $(CODE)*.c
	$(CC) $(CFLAGS) $(CLIBS) $? -o nyacate

run:
	./nyacate
