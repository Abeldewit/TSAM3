out: server client
	@echo "Files made"

server: server.cpp
	g++ -std=c++11 server.cpp -o tsamvgroup100

client: client.cpp
	g++ -std=c++11 client.cpp -lpthread -o client
