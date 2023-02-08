.PHONY=clean

# for linux/mac x86_64/amd64
jit:./src/jit.cpp ./src/amd64jit.h
	@ echo "[build] jit - amd64"
	@ clang++ -O3 -std=c++14 ./src/jit.cpp -o jit -Wshadow -Wall

# for windows mingw-w64 x86_64/amd64
jit.exe:./src/jit.cpp ./src/amd64jit.h
	@ echo "[build] jit.exe - amd64"
	@ g++ -O3 -std=c++14 ./src/jit.cpp -o jit.exe -Wshadow -Wall

# for mac arm64, x86_64/amd64 cross-platform executable
# this will be translated to arm by rosseta2 on arm64 macOS
darwin_amd64:./src/jit.cpp ./src/amd64jit.h
	@ echo "[build] jit - amd64 for darwin"
	@ clang++ -O3 -std=c++14 ./src/jit.cpp -o jit -Wshadow -Wall -target x86_64-apple-darwin

clean:
	@ echo "[clean] jit" && if [ -e jit ]; then rm jit; fi
	@ echo "[clean] jit.exe" && if [ -e jit.exe ]; then rm jit.exe; fi