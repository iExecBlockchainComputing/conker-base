CC=cc
CFLAGS += -Wall -I/usr/local/include/rats-tls/ 
LDFLAGS += -L/usr/local/lib/rats-tls/

all: quote_generator

quote_generator: src/quote_generator.c
	$(CC) src/quote_generator.c -lrats_tls -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	/bin/rm -rf *.o *~ quote_generator
