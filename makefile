all:jit.cpp
	clang++ -O3 jit.cpp -o jit -Wshadow -Wall