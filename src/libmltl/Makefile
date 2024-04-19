TARGET = main
CC = gcc
CXX = g++
CFLAGS = -std=c++17 -Wall -O3 -fopenmp -DNDEBUG -fno-rtti
# CFLAGS = -Wall -g -fno-rtti
LDFLAGS =
OBJECTS = main.o ast.cc parser.o

PYTHON_SRCS = libmltl_pybind.cc ast.cc parser.cc
PYTHON_TARGET = libmltl$(shell python3-config --extension-suffix)

.PHONY: default all clean cpp python

all: $(TARGET) $(PYTHON_TARGET)
cpp: $(TARGET)
python: $(PYTHON_TARGET)

%.o: %.cc
	$(CXX) $(CFLAGS) -c $< -o $@
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(PYTHON_TARGET): $(PYTHON_SRCS)
	$(CXX) -std=c++17 -shared -fPIC \
		$(shell python3-config --cflags --ldflags) \
		-o $@ $(PYTHON_SRCS)

clean:
	rm -f $(TARGET) $(PYTHON_TARGET) *.o *.so vgcore.*

