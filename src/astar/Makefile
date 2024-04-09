TARGET = astar
CC = gcc
CXX = g++
CFLAGS = -Wall -O3
# CFLAGS = -Wall -g
LDFLAGS =
OBJECTS = main.o


.PHONY: default all clean

all: $(TARGET)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.cc
	$(CXX) $(CFLAGS) -c $< -o $@
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) *.o

