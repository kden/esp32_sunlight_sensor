# Makefile for core business logic tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I include/
SRCDIR = main
TESTDIR = test

# Test targets
DATA_SENDER_TARGET = test_data_sender_core
SENSOR_BUFFER_TARGET = test_sensor_buffer_core

# Source files for each test suite
DATA_SENDER_SOURCES = $(SRCDIR)/data_sender_core.c $(TESTDIR)/test_data_sender_core.c
SENSOR_BUFFER_SOURCES = $(SRCDIR)/sensor_buffer_core.c $(TESTDIR)/test_sensor_buffer_core.c

# All targets
TARGETS = $(DATA_SENDER_TARGET) $(SENSOR_BUFFER_TARGET)

# Default target - run all tests
all: test

# Build and run all tests
test: $(TARGETS)
	@echo "Running data sender core tests..."
	./$(DATA_SENDER_TARGET) test-results-data-sender.xml
	@echo "Running sensor buffer core tests..."
	./$(SENSOR_BUFFER_TARGET) test-results-sensor-buffer.xml
	@echo "All tests completed!"

# Build individual test executables
$(DATA_SENDER_TARGET): $(DATA_SENDER_SOURCES)
	$(CC) $(CFLAGS) $(DATA_SENDER_SOURCES) -o $(DATA_SENDER_TARGET)

$(SENSOR_BUFFER_TARGET): $(SENSOR_BUFFER_SOURCES)
	$(CC) $(CFLAGS) $(SENSOR_BUFFER_SOURCES) -o $(SENSOR_BUFFER_TARGET)

# Run individual test suites
test-data-sender: $(DATA_SENDER_TARGET)
	@echo "Running data sender core tests..."
	./$(DATA_SENDER_TARGET) test-results-data-sender.xml

test-sensor-buffer: $(SENSOR_BUFFER_TARGET)
	@echo "Running sensor buffer core tests..."
	./$(SENSOR_BUFFER_TARGET) test-results-sensor-buffer.xml

# Clean up
clean:
	rm -f $(TARGETS) test-results-*.xml

# Run tests and generate XML only (for CI)
test-xml: $(TARGETS)
	./$(DATA_SENDER_TARGET) test-results-data-sender.xml || true
	./$(SENSOR_BUFFER_TARGET) test-results-sensor-buffer.xml || true
	@echo "Test results written to test-results-*.xml files"

# Quick test run without XML output
test-quick: $(TARGETS)
	@echo "Quick test run (no XML output)..."
	./$(DATA_SENDER_TARGET) || true
	./$(SENSOR_BUFFER_TARGET) || true

# Help
help:
	@echo "Available targets:"
	@echo "  test              - Build and run all tests (default)"
	@echo "  test-data-sender  - Run only data sender tests"
	@echo "  test-sensor-buffer- Run only sensor buffer tests"
	@echo "  test-xml          - Run all tests and generate XML output"
	@echo "  test-quick        - Quick test run without XML"
	@echo "  clean             - Remove build artifacts"
	@echo "  help              - Show this help message"
	@echo ""
	@echo "Individual build targets:"
	@echo "  $(DATA_SENDER_TARGET)   - Build data sender test executable"
	@echo "  $(SENSOR_BUFFER_TARGET) - Build sensor buffer test executable"

.PHONY: all test test-data-sender test-sensor-buffer clean test-xml test-quick help