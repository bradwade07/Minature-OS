
CFLAGS = -Wall -Wpedantic -ggdb -std=c99

all: build

build:
	gcc $(CFLAGS)  main.c list.o -o main

clean:
	rm -f main new_core*