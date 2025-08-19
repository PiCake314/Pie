
# CC = llvm-g++
CC = g++
VER = -std=c++23
ARGS = -Wall -Wextra -Wimplicit-fallthrough -Wpedantic -Wno-gnu-case-range -Wno-missing-braces # for stdx

INCLUDE = -I/usr/local/include/mp11/include/ -I/usr/local/include/cpp-std-extensions/include/


clang: main.cc
	$(CC) $(ARGS) $(VER) $(INCLUDE) main.cc -o Pie -fsanitize=address -fsanitize=undefined

