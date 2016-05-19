CC = g++ -Wall -std=c++11 -pthread

compile:
	$(CC) -o HW3_103062122_Ser HW3_103062122_Ser.cpp
	$(CC) -o HW3_103062122_Cli HW3_103062122_Cli.cpp

clean:
	rm HW3_103062122_Ser HW3_103062122_Cli
