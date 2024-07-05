CC=cc
CFLAGS = -Wall -g
LDLIBS = 
RPC_SYSTEM=rpc.o

SRC = rpc.c client.c server.c
OBJ = $(SRC:.c=.o)
 
EXE = rpc-server rpc-client

.PHONY: format all

all: $(RPC_SYSTEM) $(EXE)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) $(CFLAGS) -c -o $@ $<

# RPC_SYSTEM_A=rpc.a
# $(RPC_SYSTEM_A): rpc.o
# 	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

rpc-server: server.o rpc.o
	gcc -Wall -o rpc-server server.o rpc.o

rpc-client: client.o rpc.o
	gcc -Wall -o rpc-client client.o rpc.o

client.o: client.c rpc.h

server.o: server.c rpc.h

rpc.o: rpc.c rpc.h

format:
	clang-format -style=file -i *.c *.h

clean:
	@rm -f *.o $(EXE)