CC=cc
CFLAGS += -Wall -I/usr/local/include/rats-tls/ 
LDFLAGS += -L/usr/local/lib/rats-tls/

all: key_provider_agent

key_provider_agent: src/key_provider_agent.c
	$(CC) src/key_provider_agent.c -lcurl -lrats_tls -ljansson -o $@ $(CFLAGS) $(LDFLAGS)

clean:
	/bin/rm -rf *.o *~ key_provider_agent
