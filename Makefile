# Host (PC) build of the unit and integration tests, for machines without
# PlatformIO. PlatformIO users run `pio test -e native` instead.
#
#   make test          build and run every suite in test/test_*/
#   make test_dosing   build and run one suite
#   make clean
#
# Unity (the test framework PlatformIO uses) is fetched into .deps/ on first use.

CXX       ?= g++
# gnu++11 -pedantic: the same language level the Arduino toolchain uses.
CXXFLAGS  ?= -std=gnu++11 -pedantic -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
UNITY_DIR ?= .deps/Unity
BUILD_DIR ?= build/test

LIB_SRC := $(wildcard lib/clara_common/src/clara/*.cpp) \
           $(wildcard lib/clara_dosing/src/clara/dosing/*.cpp) \
           $(wildcard lib/clara_mainboard/src/clara/mainboard/*.cpp)
INCLUDES := -Ilib/clara_common/src -Ilib/clara_dosing/src -Ilib/clara_mainboard/src -I$(UNITY_DIR)/src
SUITES := $(notdir $(wildcard test/test_*))

.PHONY: test clean $(SUITES)

test: $(SUITES)

$(UNITY_DIR)/src/unity.c:
	git clone --depth 1 --branch v2.6.1 https://github.com/ThrowTheSwitch/Unity.git $(UNITY_DIR)

$(BUILD_DIR)/unity.o: $(UNITY_DIR)/src/unity.c
	@mkdir -p $(BUILD_DIR)
	$(CC) -O1 -g -DUNITY_INCLUDE_DOUBLE -fsanitize=address,undefined -c $< -o $@

$(SUITES): %: $(BUILD_DIR)/unity.o
	@echo "== $@"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -DUNITY_INCLUDE_DOUBLE $(LIB_SRC) $(wildcard test/$@/*.cpp) \
	    $(BUILD_DIR)/unity.o -o $(BUILD_DIR)/$@ -lm
	@./$(BUILD_DIR)/$@

clean:
	rm -rf $(BUILD_DIR)
