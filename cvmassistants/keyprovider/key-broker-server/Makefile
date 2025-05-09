CC=cc
CFLAGS += -Wall -I/usr/local/include/rats-tls/ 
LDFLAGS += -L/usr/local/lib/rats-tls/

all: key_broker_server

key_broker_server: src/key_broker_server.c
	$(CC) src/key_broker_server.c -lrats_tls -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	/bin/rm -rf *.o *~ key_broker_server
