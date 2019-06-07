.PHONY: build clean

build:
	clang src/*.c -o clox

clean:
	rm -rf src/*.h.gch
	rm -f clox
