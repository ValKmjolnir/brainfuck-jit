all:jit.cpp
	clang++ jit.cpp -o jit -Wshadow -Wall