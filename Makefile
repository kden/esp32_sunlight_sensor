
# Makefile for data sender core tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I include/
SRCDIR = main
TESTDIR = test
TARGET = test_core

# Source files
SOURCES = $(SRCDIR)/data_sender_core.c $(TESTDIR)/test_data_sender_core.c

# Default target
all: test

# Build and run tests
test: $(TARGET)
	./$(TARGET) test-results.xml

# Build test executable
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

# Clean up
clean:
	rm -f $(TARGET) test-results.xml

# Run tests and generate XML only (for CI)
test-xml: $(TARGET)
	./$(TARGET) test-results.xml || true
	@echo "Test results written to test-results.xml"

# Help
help:
	@echo "Available targets:"
	@echo "  test      - Build and run tests (default)"
	@echo "  test-xml  - Run tests and generate XML output"
	@echo "  clean     - Remove build artifacts"
	@echo "  help      - Show this help message"

.PHONY: all test clean test-xml help