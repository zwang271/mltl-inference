TARGET = astar
CC = gcc
CXX = g++
CFLAGS = -Wall -O3 -fopenmp -DNDEBUG -fno-rtti
# CFLAGS = -Wall -g -fno-rtti
LDFLAGS = -fopenmp
OBJECTS = main.o libmltl/ast.o libmltl/parser.o quine_mccluskey.o

PYTHON_SRCS = libmltl/libmltl_pybind.cc libmltl/ast.cc libmltl/parser.cc
PYTHON_TARGET = libmltl$(shell python3-config --extension-suffix)

.PHONY: default all clean cpp python

all: $(TARGET) $(PYTHON_TARGET)
cpp: $(TARGET)
python: $(PYTHON_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.cc
	$(CXX) $(CFLAGS) -c $< -o $@
%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@
%.o: %.C
	$(CXX) $(CFLAGS) -c $< -o $@
%.o: %.cxx
	$(CXX) $(CFLAGS) -c $< -o $@
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(PYTHON_TARGET): $(PYTHON_SRCS)
	$(CXX) -shared -fPIC \
		$(shell python3-config --cflags --ldflags) \
		-o $@ $(PYTHON_SRCS)

clean:
	rm -f $(TARGET) $(PYTHON_TARGET) $(OBJECTS) *.o *.so vgcore.*

