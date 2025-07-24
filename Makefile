CXX = g++
CXXFLAGS = -O2 -std=c++11
OPT = #-DNO_PERF_MONITOR
TARGET = demo
SRCS = demo.cpp perf_event_open_tool.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OPT) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(OPT) -c $<

clean:
	rm -f $(OBJS) $(TARGET)
