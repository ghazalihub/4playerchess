CXX      := g++
CXXFLAGS := -std=c++17 -O3 -march=native -Wall -Wextra -Wshadow \
            -fno-rtti -DNDEBUG
LDFLAGS  :=
TARGET   := chess4
SRCS     := main.cpp board.cpp eval.cpp search.cpp game.cpp
OBJS     := $(SRCS:.cpp=.o)

# Test and Benchmark files
TEST_SRCS := test_engine.cpp board.cpp eval.cpp search.cpp game.cpp
TEST_OBJS := $(TEST_SRCS:.cpp=.o)
TEST_TARGET := test_engine

BENCH_SRCS := benchmark.cpp board.cpp eval.cpp search.cpp game.cpp
BENCH_OBJS := $(BENCH_SRCS:.cpp=.o)
BENCH_TARGET := benchmark

.PHONY: all clean debug profile test bench selfplay gui

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  Build complete: ./$(TARGET)"
	@echo "  Usage:  ./$(TARGET) [--depth N] [--time MS]"
	@echo ""

%.o: %.cpp chess4.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

bench: $(BENCH_TARGET)
	./$(BENCH_TARGET)

$(BENCH_TARGET): $(BENCH_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

selfplay: $(TARGET)
	python3 self_play.py

gui: $(TARGET)
	python3 gui.py

debug: CXXFLAGS = -std=c++17 -O0 -g -fsanitize=address,undefined -Wall -Wextra
debug: LDFLAGS  = -fsanitize=address,undefined
debug: $(TARGET)

profile: CXXFLAGS += -pg
profile: LDFLAGS  += -pg
profile: $(TARGET)

clean:
	rm -f *.o $(TARGET) $(TEST_TARGET) $(BENCH_TARGET)
