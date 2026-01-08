
CXX = clang++

MODE ?= release
PIE = ./pie

WARN = -Wall -Wextra -Wpedantic -Wno-missing-braces
CXXFLAGS :=
CXXFLAGS += $(WARN)
CXXFLAGS += -std=c++23


REMOTE_INCLUDE_DIR = remote_includes
MP11_DIR = $(REMOTE_INCLUDE_DIR)/mp11
CPP_STD_EXT_DIR = $(REMOTE_INCLUDE_DIR)/cpp-std-extensions

INCLUDE += $(MP11_DIR)/include
INCLUDE += $(CPP_STD_EXT_DIR)/include
# INCLUDE += std
INCLUDE += include

CXXFLAGS += $(INCLUDE:%=-I%)

CXXFILES != find src -name '*.cpp'
OBJS = $(CXXFILES:%.cpp=%.o)

MODE_san = -fsanitize=address -fsanitize=undefined -g3
MODE_release = -O3 -flto
MODE_debug = -g3

ifeq ($(MODE_$(MODE)),)
$(error No such mode $(MODE))
endif

CXXFLAGS += $(MODE_$(MODE))
LDFLAGS += $(MODE_$(MODE))

default: all

all: checklibs
	$(MAKE) $(PIE)

$(PIE): $(OBJS)
	$(CXX) -o $(PIE) $(OBJS) $(LDFLAGS)

# main: main.cc
# 	@$(MAKE) checklibs
# 	$(CXX) $(CPP) $(VER) $(INCLUDE) $(OPT) main.cc -o Pie $(CFLAGS)

# debug: main.cc
# 	@$(MAKE) checklibs
# 	$(CXX) $(CPP) $(VER) $(INCLUDE) -O0 main.cc -o Pie $(SAN) $(CFLAGS)

# test: Tests/Test.cc
# 	@$(MAKE) checklibs
# 	$(CXX) $(CPP) $(VER) $(INCLUDE) -O0 Tests/Test.cc Tests/catch.cpp -o run_tests $(SAN) -DNO_ERR_LOC && ./run_tests && rm run_tests $(CFLAGS)

# gh-actions: Tests/Test.cc
# 	@$(MAKE) checklibs
# 	$(CXX) $(CPP) $(VER) $(INCLUDE) -O0 Tests/Test.cc Tests/catch.cpp -o run_tests -DNO_ERR_LOC && ./run_tests $(CFLAGS)

$(OBJS): $(@:%.o=%.cpp)
	$(CXX) -o $(@) -c $(@:%.o=%.cpp) $(CXXFLAGS)

.PHONY: checklibs
checklibs: .dummy
	@mkdir -p $(REMOTE_INCLUDE_DIR)
	@if [ ! -d "$(MP11_DIR)" ]; then \
        echo "Cloning Boost.MP11..."; \
        git clone https://github.com/boostorg/mp11 $(MP11_DIR); \
	fi
	@if [ ! -d "$(CPP_STD_EXT_DIR)" ]; then \
        echo "Cloning cpp-std-extensions..."; \
        git clone https://github.com/intel/cpp-std-extensions $(CPP_STD_EXT_DIR); \
	fi

.PHONY: clean
clean: .dummy
	rm -f Pie run_tests

.dummy:
