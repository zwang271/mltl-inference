TARGET = astar
CC = gcc
CXX = g++
CFLAGS = -Wall -O3 -fopenmp -DNDEBUG
# CFLAGS = -Wall -g
LDFLAGS = -fopenmp
# OBJECTS = main.o mltl_ast.o evaluate_mltl.o quine_mccluskey.o utils.o
OBJECTS = main.o mltl_ast.o quine_mccluskey.o


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

