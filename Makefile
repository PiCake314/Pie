
CC = g++
VER = -std=c++23
ARGS = -Wall -Wextra -Wimplicit-fallthrough -Wpedantic -Wno-gnu-case-range -Wno-missing-braces # for stdx
INCLUDE = -I/usr/local/include/mp11/include/ -I/usr/local/include/cpp-std-extensions/include/


clang: main.cc
	$(CC) $(ARGS) $(VER) $(INCLUDE) main.cc -o Pie



# CC = g++-14
# ARGS = -Wall -Wextra -Wpedantic -Wimplicit-fallthrough 
# VER = -std=c++23

# gcc: main.cc
# 	$(GCC) $(ARGS) $(VER) main.cc