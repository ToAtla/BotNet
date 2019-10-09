all: clean build runserverhardcoded

clean:
	rm -f tsamgroup33 && rm -f client && rm -f doddi_server
build:
	g++ -Wall -std=c++11 doddi_server.cpp -o doddi_server && g++ -Wall -std=c++11 doddi_client.cpp -o client
runserverhardcoded:
	./doddi_server 4039
runclienthardcoded:
	./client 130.208.243.61 4039
cli: clean build runclienthardcoded
