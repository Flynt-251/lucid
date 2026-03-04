CC = clang

CFLAGS = -g -Wall

LFLAGS = -I/usr/include/mysql -I/usr/include/mysql/mysql -L/usr/lib64 -L/usr/local/lib -lmariadb -largon2 -ljson-c -lmicrohttpd neon2

TARGET = lucid-server

lucid-server: lucid-server.c
	$(CC) $(CFLAGS) neon2.c -largon2 -c -o neon2
	$(CC) $(CFLAGS) $(TARGET).c -o $(TARGET) $(LFLAGS)

client-manager: client-manager.c
	$(CC) $(CFLAGS) neon2.c -largon2 -c -o neon2
	$(CC) $(CFLAGS) client-manager.c -o manager $(LFLAGS)