all: clean build run

clean:
	rm -f V_Group_33

build:
	g++ -Wall -std=c++11 V_Group_33.cpp -o V_Group_33
run:
	./V_Group_33
