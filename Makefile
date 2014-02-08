SERVERNAME=crater
CLIENTNAME=crater-client
CC=clang
CCFLAGS=-std=c99 -g -Wall -pedantic -D_BSD_SOURCE
LDFLAGS=-lpthread
SRCDIR=./src/
FILES=messages.c actors.c crater.c server.c
SERVERFILES=$(FILES) main.c
CLIENTFILES=$(FILES) client.c

all:
	$(CC) $(CCFLAGS) -o $(SERVERNAME) $(addprefix $(SRCDIR),$(SERVERFILES)) $(LDFLAGS)

client:
	$(CC) $(CCFLAGS) -o $(CLIENTNAME) $(addprefix $(SRCDIR),$(CLIENTFILES)) $(LDFLAGS)
