CC = clang

CFLAGS = -g -Wall

LFLAGS = -L/usr/lib64 -L/usr/local/lib -ljson-c -lcurl

TARGET = observer

observer: observer.c
	$(CC) $(CFLAGS) $(TARGET).c -o $(TARGET) $(LFLAGS)
