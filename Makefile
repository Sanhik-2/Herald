CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread -Iinclude
LDFLAGS  := -lcrypto

SRCS     := $(wildcard Src/*.cpp)
OBJS     := $(SRCS:Src/%.cpp=build/%.o)
TARGET   := herald

# Default rule (runs when you just type `make`)
all: $(TARGET)

# Link all object files into the final binary
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Pattern rule: compile any Src/<name>.cpp into build/<name>.o
build/%.o: Src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create the build/ folder if it doesn't exist
build:
	mkdir -p build

# Remove build artifacts
clean:
	rm -rf build $(TARGET)

.PHONY: all clean