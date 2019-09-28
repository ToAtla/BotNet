all: clean build runserverhardcoded

clean:
	rm -f tsamgroup33 && rm -f client
build:
	g++ -Wall -std=c++11 tsamgroup33.cpp -o tsamgroup33 && g++ -Wall -std=c++11 client.cpp -o client
runserverhardcoded:
	./tsamgroup33 4039
runclienthardcoded:
	./client 130.208.243.61 4039
cli: clean build runclienthardcoded
