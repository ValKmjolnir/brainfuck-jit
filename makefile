.PHONY=clean

jit:./src/jit.cpp ./src/amd64jit.h
	@ echo "[build] jit - amd64"
	@ clang++ -O3 -std=c++14 ./src/jit.cpp -o jit -Wshadow -Wall

jit.exe:./src/jit.cpp ./src/amd64jit.h
	@ echo "[build] jit.exe - amd64"
	@ g++ -O3 -std=c++14 ./src/jit.cpp -o jit.exe -Wshadow -Wall

clean:
	@ echo "[clean] jit" && if [ -e jit ]; then rm jit; fi
	@ echo "[clean] jit.exe" && if [ -e jit.exe ]; then rm jit.exe; fi