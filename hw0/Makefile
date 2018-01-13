CFLAGS= -g -Wall
CC= gcc
all: main map wc 

clean:
	rm -f main.o map.o wc.o main map wc

.PHONY: clean all

main: main.o
	${CC} ${CFLAGS} $< -o $@
map: map.o
	${CC} ${CFLAGS} $< -o $@
wc: wc.o
	${CC} ${CFLAGS} $< -o $@

main.o: main.c Makefile
map.o: map.c Makefile
wc.o: wc.c Makefile
