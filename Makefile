CC := gcc
CFLAGS := -Wall -Wextra -O2
INCLUDES := -I./inc

SOURCES := src/dalloc.c
TARGET := example

.PHONY: all clean example example-single help

all: example

# Build example with Multi-Heap API only
example: $(SOURCES) example.c
	$(CC) $(CFLAGS) $(INCLUDES) $(SOURCES) example.c -o $(TARGET)
	@echo "Built: $(TARGET)"
	@echo "Run with: ./$(TARGET)"

# Build example with Single-Heap API enabled
example-single: $(SOURCES) example.c
	$(CC) $(CFLAGS) $(INCLUDES) -DUSE_SINGLE_HEAP_MEMORY $(SOURCES) example.c -o $(TARGET)
	@echo "Built: $(TARGET) (with USE_SINGLE_HEAP_MEMORY)"
	@echo "Run with: ./$(TARGET)"

# Build with debug symbols
debug: CFLAGS += -g -O0
debug: example

clean:
	rm -f $(TARGET)
	rm -f *.o src/*.o

help:
	@echo "dalloc Makefile targets:"
	@echo "  make              - Build example (Multi-Heap API)"
	@echo "  make example      - Build example (Multi-Heap API)"
	@echo "  make example-single - Build example with Single-Heap API"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make clean        - Remove build artifacts"
	@echo ""
	@echo "For tests, use CMake:"
	@echo "  cd tests && mkdir build && cd build && cmake .. && make"
