all: file_server

file_server.o: file_server.cc
	g++ file_server.cc -c -g -lpthread -lstdc++fs -std=c++17

file_server: file_server.o
	g++ file_server.o -o file_server -lboost_program_options -lpthread -lstdc++fs -std=c++17

.PHONY clean:
clean:
	rm -rf file_server.o file_server
