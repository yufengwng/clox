.PHONY: default build clean test test_suite test_clean

default: build

build:
	clang -Wall -Wextra -O2 -pedantic --std=c11 src/*.c -o clox

test:
ifdef FILTER
	@ python3 test.py $(FILTER)
else
	@ python3 test.py
endif

test_suite:
	mkdir -p spec
	git clone https://github.com/munificent/craftinginterpreters ci
	cp -r ci/test/* spec
	rm -rf ci

test_clean:
	rm -rf spec

clean:
	rm -f clox
