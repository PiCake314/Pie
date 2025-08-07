
CC = g++
ARGS = -Wall -Wextra -Wimplicit-fallthrough 
# -Wpedantic -Wno-gnu-case-range
VER = -std=c++23

clang: main.cc
	$(CC) $(ARGS) $(VER) main.cc -o Pie



# CC = g++-14
# ARGS = -Wall -Wextra -Wpedantic -Wimplicit-fallthrough 
# VER = -std=c++23

# gcc: main.cc
# 	$(GCC) $(ARGS) $(VER) main.cc