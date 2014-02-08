NAME=crater
CC=clang
CCFLAGS=-std=c99 -g -Wall -pedantic -D_BSD_SOURCE
LDFLAGS=-lpthread
SRCDIR=./src/
FILES=messages.c actors.c crater.c server.c main.c

all:
	$(CC) $(CCFLAGS) -o $(NAME) $(addprefix $(SRCDIR),$(FILES)) $(LDFLAGS)

