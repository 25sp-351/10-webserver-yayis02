# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -pthread

# Target
TARGET = server

# Source files
SRCS = server.c

# Default rule
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean up
clean:
	rm -f $(TARGET)
