CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

TARGETS = parent child1 child2

all: $(TARGETS)

parent: parent.c
	$(CC) $(CFLAGS) parent.c -o build/parent

child1: child1.c
	$(CC) $(CFLAGS) child1.c -o build/child1
child2: child2.c
	$(CC) $(CFLAGS) child2.c -o build/child2

run: all
	./parent

clean:
	rm -f $(TARGETS)

.PHONY: all run clean