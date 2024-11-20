all: server client

server: server.cpp
	g++ -std=c++17 -g -pthread server.cpp -o server -lrt

client: client.cpp
	g++ -std=c++17 -g -pthread client.cpp -o client -lrt

clean:
	rm -f server client
