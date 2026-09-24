CXX = clang++
CXXFLAGS = -std=c++20 -O2 -Wall

TARGET = web/processor
SRC = cpp/processor.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
