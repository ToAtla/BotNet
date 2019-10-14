all: clean build

clean:
	rm -f client && rm -f tsamgroup75
build:
	g++ -Wall -std=c++11 tsamgroup75.cpp -o tsamgroup75 && g++ -Wall -std=c++11 client.cpp -o client
