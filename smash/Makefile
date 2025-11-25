CC = gcc
CFLAGS = -std=c99 -g -Wall -Werror -pedantic-errors -DNDEBUG -pthread
TARGET = smash

all:
	$(CC) $(CFLAGS) *.c my_system_call_c.o -o $(TARGET)

clean:
	rm -f $(TARGET) *.o
