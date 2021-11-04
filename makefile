all:jit.cpp amd64jit.h
	clang++ -O3 jit.cpp -o jit -Wshadow -Wall