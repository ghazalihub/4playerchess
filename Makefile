CXX      := g++
CXXFLAGS := -std=c++17 -O3 -march=native -Wall -Wextra -Wshadow \
            -fno-rtti -DNDEBUG
LDFLAGS  :=
TARGET   := chess4
SRCS     := main.cpp board.cpp eval.cpp search.cpp game.cpp
OBJS     := $(SRCS:.cpp=.o)

.PHONY: all clean debug profile

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  Build complete: ./$(TARGET)"
	@echo "  Usage:  ./chess4 [--depth N] [--time MS]"
	@echo ""

%.o: %.cpp chess4.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

debug: CXXFLAGS = -std=c++17 -O0 -g -fsanitize=address,undefined -Wall -Wextra
debug: LDFLAGS  = -fsanitize=address,undefined
debug: $(TARGET)

profile: CXXFLAGS += -pg
profile: LDFLAGS  += -pg
profile: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
