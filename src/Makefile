CC	= g++ 
CXXFLAGS	= -Wextra -Wall -O2 -std=c++17 -lboost_system -lboost_filesystem  -pthread -llboost_program_options

LFLAGS	= -Wall -lboost_system -lboost_filesystem -pthread -l boost_program_options

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
