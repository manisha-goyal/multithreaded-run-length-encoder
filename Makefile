CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Werror -Wextra 
LDFLAGS=-pthread

.PHONY: all
all: nyuenc

nyush: nyuenc.o

nyush.o: nyuenc.c

.PHONY: clean
clean:
	rm -f *.o nyuenc
	