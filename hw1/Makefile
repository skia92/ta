SRCS=shell.c tokenizer.c process.c
EXECUTABLES=shell

CC=gcc
CFLAGS=-g -Wall -std=gnu99
LDFLAGS=

OBJS=$(SRCS:.c=.o)

all: $(EXECUTABLES)

$(EXECUTABLES): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

sleep: sleep.o
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(EXECUTABLES) $(OBJS)
	rm -rf sleep sleep.o
