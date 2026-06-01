CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS := -pthread
TARGET := sccpp
SRCDIR := src
OBJDIR := build

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean run test debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -Ithird_party/nlohmann -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -pthread
debug: all

run: all
	./$(TARGET)

test: all
	./$(TARGET) src/

test-full: all
	@echo "=== sccpp ===" && ./$(TARGET) src/ && echo "=== scc (reference) ===" && scc src/ --no-cocomo --no-size 2>/dev/null || echo "scc not installed for comparison"

clean:
	rm -rf $(OBJDIR) $(TARGET)
