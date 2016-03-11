CC = gcc
CF = -std=c11
DEPS = build/main.o build/spyre.o

all: build/spy

build:
	mkdir build

build/spy: build $(DEPS)
	$(CC) $(CF) $(DEPS) -o spy
	mv spy /usr/local/bin
	rm -Rf build/*

build/main.o: src/main.c
	$(CC) $(CF) -c src/main.c -o build/main.o

build/spyre.o: src/interpreter/spyre.c
	$(CC) $(CF) -c src/interpreter/spyre.c -o build/spyre.o
