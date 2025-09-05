# Compiler configuration

# CC = g++-14
CC = g++
VER = -std=c++23
# OPT = -O3
ARGS = -Wall -Wextra -Wimplicit-fallthrough -Wpedantic -Wno-gnu-case-range -Wno-missing-braces -g
CPP = Type/*.cxx
SAN = -fsanitize=address -fsanitize=undefined

# Library directories
INCLUDE_DIR = includes
MP11_DIR = $(INCLUDE_DIR)/mp11
CPP_STD_EXT_DIR = $(INCLUDE_DIR)/cpp-std-extensions

# Include paths
INCLUDE = -I$(MP11_DIR)/include/ -I$(CPP_STD_EXT_DIR)/include/

# Main target
main: checklibs main.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) $(OPT) main.cc -o Pie $(SAN)


# Check and clone libraries if they don't exist
checklibs:
	@mkdir -p $(INCLUDE_DIR)
	@if [ ! -d "$(MP11_DIR)" ]; then \
        echo "Cloning Boost.MP11..."; \
        git clone https://github.com/boostorg/mp11 $(MP11_DIR); \
	fi
	@if [ ! -d "$(CPP_STD_EXT_DIR)" ]; then \
        echo "Cloning cpp-std-extensions..."; \
        git clone https://github.com/intel/cpp-std-extensions $(CPP_STD_EXT_DIR); \
	fi

# Clean up
clean:
	rm -f Pie

.PHONY: checklibs clean

