CXX := g++
CFLAGS := -std=c++17 -pedantic -Wall -fno-rtti
LDFLAGS := -L../libmltl/lib -lmltl
INCLUDES := -I../libmltl/include
ifeq ($(DEBUG), 1)
  CFLAGS += -DDEBUG -g -O0
else
  CFLAGS += -DNDEBUG -O2
endif

SRC_PATH := src
BIN_PATH := bin
OBJ_PATH := obj

# src files & obj files
SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.cc)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
HEADERS := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.hh)))

TARGET := $(BIN_PATH)/astar
# for editors using clangd
COMPILE_FLAGS := compile_flags.txt

.PHONY: default all clean libmltl

default: all
all: $(COMPILE_FLAGS) $(TARGET) libmltl

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.cc $(HEADERS) Makefile
	@mkdir -p $(OBJ_PATH)
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJ) libmltl
	@mkdir -p $(BIN_PATH)
	$(CXX) -o $(TARGET) $(OBJ) $(LDFLAGS)

FLAGS := $(CFLAGS) $(INCLUDES) $(LFLAGS)
$(COMPILE_FLAGS): Makefile
	@echo -n > $(COMPILE_FLAGS)
	@for flag in $(FLAGS); do \
	    echo "$$flag" >> $(COMPILE_FLAGS); \
	done

libmltl:
	$(MAKE) -C ../libmltl cpp DEBUG=$(DEBUG)

clean:
	rm -rf $(BIN_PATH) $(OBJ_PATH) $(COMPILE_FLAGS)
	$(MAKE) -C ../libmltl clean
