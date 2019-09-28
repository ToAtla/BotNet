all: clean build runserverhardcoded

clean:
	rm -f V_Group_33
build:
	g++ -Wall -std=c++11 V_Group_33.cpp -o V_Group_33 && g++ -Wall -std=c++11 client.cpp -o client
runserverhardcoded:
	./V_Group_33 4039
runclienthardcoded:
	./client 130.208.243.61 4039
cli: clean build runclienthardcoded
