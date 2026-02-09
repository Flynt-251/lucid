CC = clang++

CFLAGS = -g -Wall -std=c++17

LFLAGS = -L/usr/lib64 -L/usr/local/lib -lsfml-network -lsfml-system -ljson-c -lssl -lcrypto

TARGET = observer

observer: observer.cpp
	$(CC) $(CFLAGS) $(TARGET).cpp -o $(TARGET) $(LFLAGS)
