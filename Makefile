.PHONY: build clean

build:
	clang -Wall -Wextra -O2 -pedantic --std=c11 src/*.c -o clox

clean:
	rm -rf src/*.h.gch
	rm -f clox
