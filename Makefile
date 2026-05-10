# Compiler configuration

CC = g++
# CC = g++-15
WEBCC = emcc
VER = -std=c++23
OPT = -O2
ARGS = -Wall -Wextra -Wpedantic -Wno-missing-braces #-Wnrvo
WEB_ARGS = -sWASM=1 -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS='["callMain"]' -sASSERTIONS -sENVIRONMENT=web
CPP = Type/*.cxx Interp/*.cxx
SAN = -fsanitize=address -fsanitize=undefined -g3

OUTPUT_NAME = Pie
WEB_OUTPUT_NAME = Pie.js

## Library directories

# Pulled from GitHub
REMOTE_INCLUDE_DIR = remote_includes
MP11_DIR = $(REMOTE_INCLUDE_DIR)/mp11
CPP_STD_EXT_DIR = $(REMOTE_INCLUDE_DIR)/cpp-std-extensions

# Saved locally
LOCAL_INCLUDE_DIR = includes

# Include paths
INCLUDE = -I$(MP11_DIR)/include/ -I$(CPP_STD_EXT_DIR)/include/

# Main target
main: checklibs main.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) $(OPT) main.cc -o $(OUTPUT_NAME)

debug: checklibs main.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) -O0 main.cc -o $(OUTPUT_NAME) $(SAN)

test: checklibs Tests/Test.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) -O0 Tests/Test.cc Tests/catch.cpp -o run_tests $(SAN) -DNO_ERR_LOC && ./run_tests && rm run_tests

web: checklibs main.cc
	$(WEBCC) $(CPP) $(WEB_ARGS) $(VER) $(INCLUDE) $(OPT) main.cc -o $(WEB_OUTPUT_NAME) -DWEB_PIE

gh-actions: checklibs Tests/Test.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) -O0 Tests/Test.cc Tests/catch.cpp -o run_tests -DNO_ERR_LOC && ./run_tests


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


clean:
	rm -f $(OUTPUT_NAME) run_tests

.PHONY: checklibs clean

