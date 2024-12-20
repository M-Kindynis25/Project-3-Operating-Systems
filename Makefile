# Makefile for Compiling main, visitor, receptionist, monitor, initializeSM and end_day

# Compiler to use
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++11 -g

# Targets
TARGETS = main visitor receptionist monitor initializeSM end_day

# Default target
all: $(TARGETS)

# Rule to build main
main: main.o
	$(CXX) $(CXXFLAGS) -o main main.o

# Rule to build visitor
visitor: visitor.o
	$(CXX) $(CXXFLAGS) -o visitor visitor.o

# Rule to build receptionist
receptionist: receptionist.o
	$(CXX) $(CXXFLAGS) -o receptionist receptionist.o

# Rule to build monitor
monitor: monitor.o
	$(CXX) $(CXXFLAGS) -o monitor monitor.o

# Rule to build initializeSM
initializeSM: initializeSM.o
	$(CXX) $(CXXFLAGS) -o initializeSM initializeSM.o

# Rule to build end_day
end_day: end_day.o
	$(CXX) $(CXXFLAGS) -o end_day end_day.o

# Pattern rule for compiling .cpp files to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target to remove compiled binaries and object files
.PHONY: clean
clean:
	rm -f $(TARGETS) *.o

# Optional: Rebuild everything
.PHONY: rebuild
rebuild: clean all


run: all
	./main -o 5.02 -r 5.0
	make clean

run2: all
	./main -o 1.1 -r 0.001
	make clean

run3: all
	./main -o 1 -r 5.0
	make clean

run4: all
	./main -o 3 -r 0.9
	make clean

valgrind: all
	-valgrind --leak-check=full --track-origins=yes ./main -o 1.1 -r 0.001
	make clean
	

