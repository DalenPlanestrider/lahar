CC = gcc
CCFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter
LDFLAGS = -lglfw

TARGET = lahar
SOURCES = main.c
OBJECTS = $(SOURCES:.c=.o)

INCLUDES = -I.

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
