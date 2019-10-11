all: clean build runserverhardcoded

clean:
	rm -f client && rm -f P3_GROUP_75
build:
	g++ -Wall -std=c++11 P3_GROUP_75.cpp -o P3_GROUP_75 && g++ -Wall -std=c++11 doddi_client.cpp -o client
runserverhardcoded:
	./P3_GROUP_75 4040
runclienthardcoded:
	./client 130.208.243.61 4040
cli: clean build runclienthardcoded
