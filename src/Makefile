CC	= g++ 
CXXFLAGS	= -Wextra -Wall -O2 -std=c++17

LFLAGS	= -Wall -lboost_system -lboost_filesystem -lboost_program_options -pthread

all: TARGET

debug: CFLAGS += -DDEBUG -g
debug: TARGET

TARGET: netstore-server netstore-client

netstore-server: server.o cmd.o aux.o sockets.o
	$(CC) $(LFLAGS) $^ -o $@

netstore-client: client.o cmd.o aux.o sockets.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f netstore-server netstore-client *.o *~ *.bak
