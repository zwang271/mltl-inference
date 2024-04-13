TARGET = astar
CC = gcc
CXX = g++
CFLAGS = -Wall -O3 -fopenmp -DNDEBUG
# CFLAGS = -Wall -g -fno-rtti
LDFLAGS = -fopenmp
# OBJECTS = main.o libmltl.o quine_mccluskey.o
OBJECTS = main.o libmltl.o quine_mccluskey.o utils.o evaluate_mltl.o

PYBIND_SRCS = libmltl_pybind.cc libmltl.cc
PYBIND_TARGET = libmltl$(shell python3-config --extension-suffix)

.PHONY: default all clean

all: $(TARGET) $(PYBIND_TARGET)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.cc %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(PYBIND_TARGET): $(PYBIND_SRCS)
	$(CXX) $(CFLAGS) -shared -fPIC \
		$(shell python3-config --cflags --ldflags) \
		-o $@ $(PYBIND_SRCS)

clean:
	rm -f $(TARGET) $(PYBIND_TARGET) *.o *.so vgcore.*

