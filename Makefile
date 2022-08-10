CC=gcc
CFLAGS=-Wall -Wextra -g
CLIBS=-lstrophe -lnotcurses -lnotcurses-core
CODE=src/

all: $(CODE)*.c
	$(CC) $(CFLAGS) $(CLIBS) $? -o nyacate

run:
	./nyacate
