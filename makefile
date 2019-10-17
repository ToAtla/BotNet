all: clean build

clean:
	rm -f client && rm -f server
build:
	g++ -Wall -std=c++11 server.cpp -o server && g++ -Wall -std=c++11 client.cpp -o client
