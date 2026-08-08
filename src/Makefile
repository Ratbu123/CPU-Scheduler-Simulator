CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
TARGET = scheduler
SRC = scheduler.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run1: $(TARGET)
	./$(TARGET) 1

run2: $(TARGET)
	./$(TARGET) 2

run3: $(TARGET)
	./$(TARGET) 3

.PHONY: all clean run1 run2 run3
