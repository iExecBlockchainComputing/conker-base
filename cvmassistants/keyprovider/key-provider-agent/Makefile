CC=cc
CFLAGS += -Wall

all: key_provider_agent

key_provider_agent: src/key_provider_agent.c
	$(CC) src/key_provider_agent.c -lcurl -o $@ $(CFLAGS)

clean:
	/bin/rm -rf *.o *~ key_provider_agent
