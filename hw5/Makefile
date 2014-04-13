CFLAGS= -g

all: session client 

clean: 
	rm -rf session client *.o 

session: session.o help.o rbtree.o  
	gcc $(CFLAGS) session.o help.o rbtree.o -o session -lssl -lnsl -lpthread 
session.o: session.c help.h rbtree.h 

client: client.o help.o 
	gcc $(CFLAGS) client.o help.o -o client -lssl -lnsl -lpthread 
	
client.o: client.c help.h 
help.o: help.c help.h
rbtree.o: rbtree.c rbtree.h help.h 
