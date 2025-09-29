# Compiler configuration

# CC = g++-14
CC = g++
VER = -std=c++23
OPT = -O3
ARGS = -Wall -Wextra -Wpedantic -Wimplicit-fallthrough -Wno-missing-braces #-Wnrvo
CPP = Type/*.cxx
SAN = -fsanitize=address -fsanitize=undefined

## Library directories

# Pulled from GitHub
REMOTE_INCLUDE_DIR = remote_includes
MP11_DIR = $(REMOTE_INCLUDE_DIR)/mp11
CPP_STD_EXT_DIR = $(REMOTE_INCLUDE_DIR)/cpp-std-extensions

# Saved locally
LOCAL_INCLUDE_DIR = includes
BIGINT_DIR = $(LOCAL_INCLUDE_DIR)/BigInt

# Include paths
INCLUDE = -I$(MP11_DIR)/include/ -I$(CPP_STD_EXT_DIR)/include/ -I$(BIGINT_DIR)/include/

# Main target
main: checklibs main.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) $(OPT) main.cc -o Pie $(SAN)


test: checklibs Tests/Test.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) $(OPT) Tests/Test.cc Tests/catch.cpp -o run_tests $(SAN) && ./run_tests

# Check and clone libraries if they don't exist
checklibs:
	@mkdir -p $(REMOTE_INCLUDE_DIR)
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
	rm -f Pie run_tests

.PHONY: checklibs clean

