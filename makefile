CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c17 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lpthread
TARGET = autostart
SOURCES = autostart.c
OBJ = autostart.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(OBJ): $(SOURCES)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJ)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
