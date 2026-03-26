CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
AR       := ar

# Include paths
INC := -I mcptoolkit/include \
       -I mcptoolkit/include/json \
       -I mcptoolkit \
       -I TestMcp

BUILD_DIR := build

# ---------------------------------------------------------------------------
# mcptoolkit static library
# ---------------------------------------------------------------------------
LIB_SRCS := mcptoolkit/src/json/json_msg.cpp \
            mcptoolkit/src/mcp_adapter.cpp

LIB_OBJS := $(patsubst mcptoolkit/src/%.cpp, $(BUILD_DIR)/lib/%.o, $(LIB_SRCS))
LIB_OUT  := $(BUILD_DIR)/libmcptoolkit.a

# ---------------------------------------------------------------------------
# TestMcp executable
# ---------------------------------------------------------------------------
TEST_SRCS := TestMcp/TestMcp.cpp
TEST_OBJS := $(patsubst TestMcp/%.cpp, $(BUILD_DIR)/test/%.o, $(TEST_SRCS))
TEST_OUT  := $(BUILD_DIR)/testmcp

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all lib test clean

all: lib test

lib: $(LIB_OUT)

test: $(TEST_OUT)

$(LIB_OUT): $(LIB_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(BUILD_DIR)/lib/%.o: mcptoolkit/src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

$(TEST_OUT): $(TEST_OBJS) $(LIB_OUT)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/test/%.o: TestMcp/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
