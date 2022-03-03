CPPFLAGS = -g -Wall -Wextra
CPP = g++

build: server subscriber

server : server.cpp

subscriber : subscriber.cpp

.PHONY: clean

clean:
	rm -f server subscriber