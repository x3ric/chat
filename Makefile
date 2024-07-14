CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS =
TARGET = chat
SRC = ./src/*.c
PREFIX = /usr/local

.PHONY: all clean install uninstall run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: clean all
	./$(TARGET)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
