
# CC = llvm-g++
CC = g++
VER = -std=c++23
OPT = -O3
ARGS = -Wall -Wextra -Wimplicit-fallthrough -Wpedantic -Wno-gnu-case-range -Wno-missing-braces # for stdx
# SAN = -fsanitize=address -fsanitize=undefined

INCLUDE = -I/usr/local/include/mp11/include/ -I/usr/local/include/cpp-std-extensions/include/


clang: main.cc
	$(CC) $(ARGS) $(VER) $(INCLUDE) $(OPT) main.cc -o Pie $(SAN)

