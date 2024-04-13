TARGET = astar
CC = gcc
CXX = g++
CFLAGS = -Wall -O3 -fopenmp -DNDEBUG -fno-rtti
# CFLAGS = -Wall -g -fno-rtti -fno-exceptions
LDFLAGS = -fopenmp
OBJECTS = main.o libmltl.o quine_mccluskey.o


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

