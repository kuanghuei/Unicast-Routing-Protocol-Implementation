TARGETS=manager linkstate distvec

CC=g++
CCOPTS=-lpthread 

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

%: %.cpp
	$(CC) $(CCOPTS) -o $@ $< 
