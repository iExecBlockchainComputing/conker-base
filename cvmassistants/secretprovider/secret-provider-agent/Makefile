CC=cc
CFLAGS += -Wall -I/usr/local/include/rats-tls/ 
LDFLAGS += -L/usr/local/lib/rats-tls/

all: secret_provider_agent

secret_provider_agent: src/secret_provider_agent.c
	$(CC) src/secret_provider_agent.c -lrats_tls -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	/bin/rm -rf *.o *~ secret_provider_agent
