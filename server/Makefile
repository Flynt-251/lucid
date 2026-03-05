CC = clang

CFLAGS = -g -Wall

LFLAGS = -I/usr/include/mysql -I/usr/include/mysql/mysql -L/usr/lib64 -L/usr/local/lib -lmariadb -largon2 -ljson-c -lmicrohttpd neon2

NEON = neon2
SERVER = lucid-server
CLIENTS = client-manager
ADMIN = administrator-panel

lucid-server: lucid-server.c
	$(CC) $(CFLAGS) $(NEON).c -largon2 -c -o $(NEON)
	$(CC) $(CFLAGS) $(SERVER).c -o $(SERVER) $(LFLAGS)

client-manager: client-manager.c
	$(CC) $(CFLAGS) $(NEON).c -largon2 -c -o $(NEON)
	$(CC) $(CFLAGS) $(CLIENTS).c -o $(CLIENTS) $(LFLAGS)

administrator-panel: administrator-panel.c
	$(CC) $(CFLAGS) $(NEON).c -largon2 -c -o $(NEON)
	$(CC) $(CFLAGS) $(ADMIN).c -o $(ADMIN) $(LFLAGS)

all: lucid-server.c client-manager.c administrator-panel.c
	$(CC) $(CFLAGS) $(NEON).c -largon2 -c -o $(NEON)
	$(CC) $(CFLAGS) $(SERVER).c -o $(SERVER) $(LFLAGS)
	$(CC) $(CFLAGS) $(CLIENTS).c -o $(CLIENTS) $(LFLAGS)
	$(CC) $(CFLAGS) $(ADMIN).c -o $(ADMIN) $(LFLAGS)

clean:
	rm -f $(NEON) $(SERVER) $(CLIENTS) $(ADMIN)

refresh:
	make clean
	make all