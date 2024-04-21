CC := g++
CFLAGS := -std=c++17 -pedantic -Wall -O2 -fno-rtti
LDFLAGS := -L../lib -lmltl
INCLUDES := -I../include

TARGET := example

.PHONY: all clean

all: $(TARGET)

$(TARGET): example.cc
	$(CXX) $(CFLAGS) -o $@ $^ $(INCLUDES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

