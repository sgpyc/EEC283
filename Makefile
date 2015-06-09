

target : ./EEC283 

./EEC283 : main.cpp module.hpp net.hpp tree.hpp
	g++ -Wall -fopenmp -lgomp main.cpp -o ./EEC283 -g -O3

test : ./EEC283 test_input.txt
	./EEC283 test_input.txt
