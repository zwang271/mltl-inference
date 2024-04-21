CC := g++
CFLAGS := -std=c++17 -pedantic -Wall -O2 -fno-rtti
LDFLAGS := -L../../lib -lmltl
INCLUDES := -I../../include

TARGET := regression
RESULTS := results.txt

.PHONY: all clean test reference

all: $(TARGET)

$(TARGET): regression.cc
	$(CXX) $(CFLAGS) -o $@ $^ $(INCLUDES) $(LDFLAGS)

# run regression tests (compares to reference file results.txt)
test: $(TARGET)
	gzip -dkf $(RESULTS).gz
	./$(TARGET) -r $(RESULTS)

# run regression tests (saves results to reference file)
reference: $(TARGET)
	./$(TARGET) -o $(RESULTS)
	gzip -k9 $(RESULTS)

clean:
	rm -f $(TARGET) $(RESULTS)
