TARGET = astar
CC = gcc
CXX = g++
CFLAGS = -Wall -O3
# CFLAGS = -Wall -g
LDFLAGS =
OBJECTS = main.o evaluate_mltl.o utils.o


.PHONY: default all clean

all: $(TARGET)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) *.o

