CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -pthread -Iinclude
LDFLAGS  := -pthread

TARGET   := lds
SRC_DIR  := src
TEST_DIR := test
BIN_DIR  := bin

SRCS := \
	$(SRC_DIR)/DriverData.cpp \
	$(SRC_DIR)/IDriverComm.cpp \
	$(SRC_DIR)/IStorage.cpp \
	$(SRC_DIR)/LocalStorage.cpp \
	$(SRC_DIR)/NBDDriverComm.cpp \
	$(TEST_DIR)/LDS.cpp

OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean re run dirs

all: dirs $(BIN_DIR)/$(TARGET)

dirs:
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o
	rm -f $(TEST_DIR)/*.o
	rm -f $(BIN_DIR)/$(TARGET)

re: clean all

run: all
	sudo ./$(BIN_DIR)/$(TARGET) /dev/nbd0 134217728