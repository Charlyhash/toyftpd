.PHONY: clean
CC=gcc
CFLAGS=-Wall -g
BIN=toyftpd
OBJS=main.o sysutil.o session.o privparent.o ftpproto.o str.o tunable.o parseconf.o privsock.o

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ -lcrypt

$.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(BIN)
