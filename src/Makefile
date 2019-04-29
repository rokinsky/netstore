CC	= g++ 
CFLAGS	= -Wextra -Wall -O2 -std=c++17
LFLAGS	= -Wall

all: TARGET

debug: CFLAGS += -DDEBUG -g
debug: TARGET

TARGET: netstore-server netstore-client

netstore-server: netstore-server.o
	$(CC) $(LFLAGS) $^ -o $@

netstore-client: netstore-client.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f netstore-server netstore-client *.o *~ *.bak
